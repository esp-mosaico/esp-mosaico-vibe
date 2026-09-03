#include "factory_system_update.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#if CONFIG_GET_STARTED_RECOVERY && CONFIG_ESP_IRIS_SYSTEM_UPDATE && \
    CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_BACKEND

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_check.h"
#include "esp_flash.h"
#include "esp_flash_partitions.h"
#include "esp_heap_caps.h"
#include "esp_iris_system_update.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "factory_system_metadata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"

#define FACTORY_SYSTEM_SCHEMA "esp-iris-system-update/v1"
#define FACTORY_SYSTEM_HASH_CHUNK_BYTES 1024U
#define FACTORY_SYSTEM_RESTART_DELAY_MS 1800U
#define FACTORY_SYSTEM_ESP32S31_CHIP_ID 0x20U
#define FACTORY_SYSTEM_FLASH_SECTOR_BYTES 0x1000U

typedef struct {
    esp_iris_system_update_component_t descriptor;
    char filename[FACTORY_SYSTEM_UPDATE_FILENAME_BYTES];
    bool completed;
} factory_update_plan_component_t;

typedef struct {
    bool prepared;
    factory_update_plan_component_t
        plan[CONFIG_ESP_IRIS_SYSTEM_UPDATE_MAX_COMPONENTS];
    size_t plan_count;
    int active_index;
    uint32_t received;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    uint8_t current_layout_sha256[ESP_IRIS_SYSTEM_SHA256_BYTES];
    uint8_t target_layout_sha256[ESP_IRIS_SYSTEM_SHA256_BYTES];
    uint8_t *bootloader_image;
    uint8_t *partition_table_image;
    const esp_partition_t *ota_partition;
    bool application_received;
} factory_update_state_t;

static const char *TAG = "factory_sysupdate";
static factory_update_state_t s_update = {
    .active_index = -1,
};
static factory_system_update_owner_t s_owner =
    FACTORY_SYSTEM_UPDATE_OWNER_NONE;
static factory_system_update_status_t s_status = {
    .owner = FACTORY_SYSTEM_UPDATE_OWNER_NONE,
    .update = {
        .phase = ESP_IRIS_SYSTEM_UPDATE_PHASE_IDLE,
        .result = ESP_OK,
    },
};
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static bool update_owner_claim(factory_system_update_owner_t owner)
{
    bool claimed = false;
    taskENTER_CRITICAL(&s_state_lock);
    if (s_owner == FACTORY_SYSTEM_UPDATE_OWNER_NONE) {
        s_owner = owner;
        claimed = true;
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return claimed;
}

static void update_owner_release(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_owner = FACTORY_SYSTEM_UPDATE_OWNER_NONE;
    taskEXIT_CRITICAL(&s_state_lock);
}

static bool update_owner_is(factory_system_update_owner_t owner)
{
    bool matches;
    taskENTER_CRITICAL(&s_state_lock);
    matches = s_owner == owner;
    taskEXIT_CRITICAL(&s_state_lock);
    return matches;
}

static void update_status_start(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    size_t component_count)
{
    taskENTER_CRITICAL(&s_state_lock);
    memset(&s_status, 0, sizeof(s_status));
    s_status.owner = owner;
    memcpy(s_status.update.operation_id, operation_id,
           sizeof(s_status.update.operation_id));
    s_status.update.phase = ESP_IRIS_SYSTEM_UPDATE_PHASE_PREPARED;
    s_status.update.component_count = (uint8_t)component_count;
    s_status.update.result = ESP_OK;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void update_status_component(uint8_t component_id, uint32_t received,
                                    uint32_t size,
                                    esp_iris_system_update_phase_t phase)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_status.update.active_component_id = component_id;
    s_status.update.component_received = received;
    s_status.update.component_size = size;
    s_status.update.phase = phase;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void update_status_component_complete(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    ++s_status.update.completed_components;
    s_status.update.active_component_id = 0;
    s_status.update.phase =
        ESP_IRIS_SYSTEM_UPDATE_PHASE_COMPONENT_VERIFIED;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void update_status_finish(esp_iris_system_update_phase_t phase,
                                 esp_err_t result)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_status.update.phase = phase;
    s_status.update.result = result;
    s_status.update.active_component_id = 0;
    taskEXIT_CRITICAL(&s_state_lock);
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        size_t size)
{
    uint8_t difference = 0;
    for (size_t i = 0; i < size; ++i) {
        difference |= left[i] ^ right[i];
    }
    return difference == 0;
}

static esp_err_t decode_hex(const char *hex, uint8_t *output,
                            size_t output_capacity, size_t *output_size)
{
    if (hex == NULL || output == NULL || output_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t length = strlen(hex);
    if (length == 0 || (length & 1U) != 0 ||
        length / 2U > output_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < length / 2U; ++i) {
        uint8_t value = 0;
        for (size_t nibble = 0; nibble < 2; ++nibble) {
            const char character = hex[i * 2U + nibble];
            uint8_t digit;
            if (character >= '0' && character <= '9') {
                digit = (uint8_t)(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                digit = (uint8_t)(character - 'a' + 10);
            } else if (character >= 'A' && character <= 'F') {
                digit = (uint8_t)(character - 'A' + 10);
            } else {
                return ESP_ERR_INVALID_ARG;
            }
            value = (uint8_t)((value << 4U) | digit);
        }
        output[i] = value;
    }
    *output_size = length / 2U;
    return ESP_OK;
}

static esp_err_t hash_flash_region(uint32_t address, size_t size,
                                   uint8_t output[32])
{
    uint8_t buffer[FACTORY_SYSTEM_HASH_CHUNK_BYTES];
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    if (psa_crypto_init() != PSA_SUCCESS ||
        psa_hash_setup(&operation, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        return ESP_FAIL;
    }
    esp_err_t err = ESP_OK;
    for (size_t offset = 0; offset < size;) {
        size_t chunk = size - offset;
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        err = esp_flash_read(NULL, buffer, address + offset, chunk);
        if (err != ESP_OK ||
            psa_hash_update(&operation, buffer, chunk) != PSA_SUCCESS) {
            err = err == ESP_OK ? ESP_FAIL : err;
            break;
        }
        offset += chunk;
    }
    size_t actual_size = 0;
    if (err == ESP_OK &&
        (psa_hash_finish(&operation, output, 32, &actual_size) != PSA_SUCCESS ||
         actual_size != 32)) {
        err = ESP_FAIL;
    }
    if (err != ESP_OK) {
        (void)psa_hash_abort(&operation);
    }
    return err;
}

static esp_err_t hash_memory(const uint8_t *data, size_t size,
                             uint8_t output[32])
{
    size_t output_size = 0;
    return psa_crypto_init() == PSA_SUCCESS &&
                   psa_hash_compute(PSA_ALG_SHA_256, data, size, output, 32,
                                    &output_size) == PSA_SUCCESS &&
                   output_size == 32
        ? ESP_OK
        : ESP_FAIL;
}

static void update_state_reset(void)
{
    free(s_update.bootloader_image);
    free(s_update.partition_table_image);
    s_update.bootloader_image = NULL;
    s_update.partition_table_image = NULL;
    s_update.prepared = false;
    s_update.plan_count = 0;
    s_update.active_index = -1;
    s_update.received = 0;
    s_update.ota_partition = NULL;
    s_update.application_received = false;
    memset(s_update.plan, 0, sizeof(s_update.plan));
    memset(s_update.operation_id, 0, sizeof(s_update.operation_id));
    memset(s_update.current_layout_sha256, 0,
           sizeof(s_update.current_layout_sha256));
    memset(s_update.target_layout_sha256, 0,
           sizeof(s_update.target_layout_sha256));
}

static const cJSON *json_member(const cJSON *object, const char *name)
{
    return cJSON_IsObject(object)
        ? cJSON_GetObjectItemCaseSensitive(object, name)
        : NULL;
}

static esp_err_t json_string(const cJSON *object, const char *name,
                             const char **value)
{
    const cJSON *item = json_member(object, name);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *value = item->valuestring;
    return ESP_OK;
}

static esp_err_t json_uint32(const cJSON *object, const char *name,
                             uint32_t maximum, uint32_t *value)
{
    const cJSON *item = json_member(object, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 ||
        item->valuedouble > maximum) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t converted = (uint32_t)item->valuedouble;
    if ((double)converted != item->valuedouble) {
        return ESP_ERR_INVALID_ARG;
    }
    *value = converted;
    return ESP_OK;
}

static esp_err_t json_sha256(const cJSON *object, const char *name,
                             uint8_t output[32])
{
    const char *hex = NULL;
    ESP_RETURN_ON_ERROR(json_string(object, name, &hex), TAG,
                        "manifest %s", name);
    size_t output_size = 0;
    ESP_RETURN_ON_ERROR(decode_hex(hex, output, 32, &output_size), TAG,
                        "manifest %s", name);
    return output_size == 32 ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static bool source_layout_authorized(const cJSON *root,
                                     const uint8_t actual[32])
{
    const cJSON *sources = json_member(root, "source_layout_sha256");
    if (!cJSON_IsArray(sources) || cJSON_GetArraySize(sources) == 0) {
        return false;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, sources) {
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
            return false;
        }
        uint8_t expected[32];
        size_t expected_size = 0;
        if (decode_hex(item->valuestring, expected, sizeof(expected),
                       &expected_size) == ESP_OK &&
            expected_size == sizeof(expected) &&
            bytes_equal(expected, actual, sizeof(expected))) {
            return true;
        }
    }
    return false;
}

static esp_err_t component_kind(const char *name,
                                esp_iris_system_update_component_kind_t *kind)
{
    if (strcmp(name, "application") == 0) {
        *kind = ESP_IRIS_SYSTEM_UPDATE_COMPONENT_APPLICATION;
    } else if (strcmp(name, "bootloader") == 0) {
        *kind = ESP_IRIS_SYSTEM_UPDATE_COMPONENT_BOOTLOADER;
    } else if (strcmp(name, "partition_table") == 0) {
        *kind = ESP_IRIS_SYSTEM_UPDATE_COMPONENT_PARTITION_TABLE;
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static bool parse_version_triplet(const char *text, uint32_t version[3])
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }
    const char *cursor = text;
    for (size_t part = 0; part < 3; ++part) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        uint32_t value = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            const uint32_t digit = (uint32_t)(*cursor - '0');
            if (value > (UINT32_MAX - digit) / 10U) {
                return false;
            }
            value = value * 10U + digit;
            ++cursor;
        }
        version[part] = value;
        if (part < 2) {
            if (*cursor != '.') {
                return false;
            }
            ++cursor;
        }
    }
    return *cursor == '\0' || *cursor == '-' || *cursor == '+';
}

static bool recovery_version_satisfies(const char *minimum)
{
    uint32_t required[3];
    uint32_t current[3];
    if (!parse_version_triplet(minimum, required) ||
        !parse_version_triplet(esp_app_get_description()->version, current)) {
        return false;
    }
    for (size_t i = 0; i < 3; ++i) {
        if (current[i] != required[i]) {
            return current[i] > required[i];
        }
    }
    return true;
}

static esp_err_t authorize_component_target(
    const esp_iris_system_update_component_t *component)
{
    if (component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_BOOTLOADER) {
        return component->target_offset == CONFIG_BOOTLOADER_OFFSET_IN_FLASH &&
                       component->size == CONFIG_PARTITION_TABLE_OFFSET -
                                              CONFIG_BOOTLOADER_OFFSET_IN_FLASH
            ? ESP_OK
            : ESP_ERR_INVALID_SIZE;
    }
    if (component->kind ==
        ESP_IRIS_SYSTEM_UPDATE_COMPONENT_PARTITION_TABLE) {
        return component->target_offset == CONFIG_PARTITION_TABLE_OFFSET &&
                       component->size == FACTORY_SYSTEM_FLASH_SECTOR_BYTES
            ? ESP_OK
            : ESP_ERR_INVALID_SIZE;
    }
    if (component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_APPLICATION) {
        const esp_partition_t *partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota_0");
        if (partition == NULL ||
            component->target_offset != partition->address ||
            component->size > partition->size) {
            return ESP_ERR_INVALID_SIZE;
        }
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t parse_component(const cJSON *item,
                                 factory_update_plan_component_t *plan)
{
    uint32_t id = 0;
    uint32_t flags = 0;
    const char *kind_name = NULL;
    const char *filename = NULL;
    ESP_RETURN_ON_ERROR(json_uint32(item, "id", UINT8_MAX, &id), TAG,
                        "component id");
    ESP_RETURN_ON_FALSE(id != 0, ESP_ERR_INVALID_ARG, TAG,
                        "component id is zero");
    ESP_RETURN_ON_ERROR(json_string(item, "kind", &kind_name), TAG,
                        "component kind");
    const cJSON *flags_item = json_member(item, "flags");
    if (flags_item != NULL) {
        ESP_RETURN_ON_ERROR(json_uint32(item, "flags", UINT16_MAX, &flags),
                            TAG, "component flags");
    }
    uint32_t target_offset = 0;
    uint32_t size = 0;
    ESP_RETURN_ON_ERROR(json_uint32(item, "target_offset", UINT32_MAX,
                                    &target_offset), TAG,
                        "component target");
    ESP_RETURN_ON_ERROR(json_uint32(item, "size", UINT32_MAX, &size), TAG,
                        "component size");
    ESP_RETURN_ON_FALSE(size != 0, ESP_ERR_INVALID_SIZE, TAG,
                        "empty component");

    memset(plan, 0, sizeof(*plan));
    plan->descriptor.id = (uint8_t)id;
    plan->descriptor.flags = (uint16_t)flags;
    plan->descriptor.target_offset = target_offset;
    plan->descriptor.size = size;
    ESP_RETURN_ON_ERROR(component_kind(kind_name, &plan->descriptor.kind), TAG,
                        "component kind policy");
    ESP_RETURN_ON_ERROR(json_sha256(item, "sha256", plan->descriptor.sha256),
                        TAG, "component digest");
    ESP_RETURN_ON_ERROR(json_string(item, "file", &filename), TAG,
                        "component file");
    const size_t filename_size = strnlen(
        filename, FACTORY_SYSTEM_UPDATE_FILENAME_BYTES);
    ESP_RETURN_ON_FALSE(
        filename_size > 0 &&
            filename_size < FACTORY_SYSTEM_UPDATE_FILENAME_BYTES &&
            strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0 &&
            strchr(filename, '/') == NULL && strchr(filename, '\\') == NULL,
        ESP_ERR_INVALID_ARG, TAG, "component file is not root-level");
    strlcpy(plan->filename, filename, sizeof(plan->filename));
    return authorize_component_target(&plan->descriptor);
}

static esp_err_t parse_manifest_json(
    const esp_iris_system_update_manifest_t *manifest)
{
    char *json = malloc(manifest->manifest_size + 1U);
    ESP_RETURN_ON_FALSE(json != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate manifest");
    memcpy(json, manifest->manifest, manifest->manifest_size);
    json[manifest->manifest_size] = '\0';
    const char *parse_end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(
        json, manifest->manifest_size + 1U, &parse_end, true);
    if (root == NULL || parse_end != json + manifest->manifest_size) {
        cJSON_Delete(root);
        free(json);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    const char *schema = NULL;
    uint32_t chip_id = 0;
    uint32_t flash_size = 0;
    uint32_t actual_flash_size = 0;
    const cJSON *target = json_member(root, "target");
    const cJSON *components = json_member(root, "components");
    const cJSON *minimum_recovery =
        json_member(root, "minimum_recovery_version");

    if (json_string(root, "schema", &schema) != ESP_OK ||
        strcmp(schema, FACTORY_SYSTEM_SCHEMA) != 0 ||
        json_member(root, "signature") != NULL ||
        json_uint32(target, "chip_id", UINT16_MAX, &chip_id) != ESP_OK ||
        chip_id != FACTORY_SYSTEM_ESP32S31_CHIP_ID ||
        json_uint32(target, "flash_size", UINT32_MAX, &flash_size) != ESP_OK ||
        esp_flash_get_size(NULL, &actual_flash_size) != ESP_OK ||
        flash_size != actual_flash_size || !cJSON_IsArray(components) ||
        cJSON_GetArraySize(components) != manifest->component_count ||
        manifest->component_count == 0 ||
        manifest->component_count >
            CONFIG_ESP_IRIS_SYSTEM_UPDATE_MAX_COMPONENTS) {
        err = ESP_ERR_INVALID_ARG;
        goto done;
    }
    if (minimum_recovery != NULL &&
        (!cJSON_IsString(minimum_recovery) ||
         !recovery_version_satisfies(minimum_recovery->valuestring))) {
        err = ESP_ERR_INVALID_VERSION;
        goto done;
    }
    err = hash_flash_region(CONFIG_PARTITION_TABLE_OFFSET,
                            FACTORY_SYSTEM_FLASH_SECTOR_BYTES,
                            s_update.current_layout_sha256);
    if (err != ESP_OK ||
        !source_layout_authorized(root, s_update.current_layout_sha256)) {
        err = err == ESP_OK ? ESP_ERR_INVALID_VERSION : err;
        goto done;
    }
    err = json_sha256(root, "target_layout_sha256",
                      s_update.target_layout_sha256);
    if (err != ESP_OK) {
        goto done;
    }

    bool seen_kinds[ESP_IRIS_SYSTEM_UPDATE_COMPONENT_DATA + 1] = {false};
    const cJSON *component = NULL;
    cJSON_ArrayForEach(component, components) {
        factory_update_plan_component_t *plan =
            &s_update.plan[s_update.plan_count];
        err = parse_component(component, plan);
        if (err != ESP_OK || seen_kinds[plan->descriptor.kind]) {
            err = err == ESP_OK ? ESP_ERR_INVALID_ARG : err;
            goto done;
        }
        for (size_t i = 0; i < s_update.plan_count; ++i) {
            if (s_update.plan[i].descriptor.id == plan->descriptor.id ||
                strcmp(s_update.plan[i].filename, plan->filename) == 0) {
                err = ESP_ERR_INVALID_ARG;
                goto done;
            }
        }
        seen_kinds[plan->descriptor.kind] = true;
        ++s_update.plan_count;
    }
    if (seen_kinds[ESP_IRIS_SYSTEM_UPDATE_COMPONENT_PARTITION_TABLE]) {
        for (size_t i = 0; i < s_update.plan_count; ++i) {
            if (s_update.plan[i].descriptor.kind ==
                    ESP_IRIS_SYSTEM_UPDATE_COMPONENT_PARTITION_TABLE &&
                !bytes_equal(s_update.plan[i].descriptor.sha256,
                             s_update.target_layout_sha256, 32)) {
                err = ESP_ERR_INVALID_CRC;
                goto done;
            }
        }
    } else if (!bytes_equal(s_update.current_layout_sha256,
                            s_update.target_layout_sha256, 32)) {
        err = ESP_ERR_INVALID_VERSION;
        goto done;
    }

done:
    cJSON_Delete(root);
    free(json);
    return err;
}

static esp_err_t prepare_update_owned(
    const esp_iris_system_update_manifest_t *manifest,
    factory_system_update_owner_t owner)
{
    ESP_RETURN_ON_FALSE(update_owner_claim(owner), ESP_ERR_INVALID_STATE, TAG,
                        "system update writer is busy");
    update_state_reset();
    if (manifest == NULL || manifest->manifest == NULL ||
        manifest->manifest_size == 0 || manifest->signature_size != 0) {
        update_status_finish(ESP_IRIS_SYSTEM_UPDATE_PHASE_FAILED,
                             ESP_ERR_INVALID_ARG);
        update_owner_release();
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t parse_err = parse_manifest_json(manifest);
    if (parse_err != ESP_OK) {
        update_status_finish(ESP_IRIS_SYSTEM_UPDATE_PHASE_FAILED, parse_err);
        update_state_reset();
        update_owner_release();
        return parse_err;
    }
    memcpy(s_update.operation_id, manifest->operation_id,
           sizeof(s_update.operation_id));
    s_update.prepared = true;
    update_status_start(owner, manifest->operation_id, s_update.plan_count);
    ESP_LOGW(TAG, "accepted unsigned system plan with %u component(s)",
             (unsigned)s_update.plan_count);
    return ESP_OK;
}

static esp_err_t prepare_update(
    const esp_iris_system_update_manifest_t *manifest, void *user_ctx)
{
    (void)user_ctx;
    return prepare_update_owned(manifest,
                                FACTORY_SYSTEM_UPDATE_OWNER_ESP_IRIS);
}

static int find_plan_component(
    const esp_iris_system_update_component_t *component)
{
    for (size_t i = 0; i < s_update.plan_count; ++i) {
        const esp_iris_system_update_component_t *expected =
            &s_update.plan[i].descriptor;
        if (expected->id == component->id &&
            expected->kind == component->kind &&
            expected->flags == component->flags &&
            expected->target_offset == component->target_offset &&
            expected->size == component->size &&
            bytes_equal(expected->sha256, component->sha256, 32)) {
            return (int)i;
        }
    }
    return -1;
}

static esp_err_t begin_component(
    const esp_iris_system_update_component_t *component, void *user_ctx)
{
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(component != NULL && s_update.prepared &&
                            s_update.active_index < 0,
                        ESP_ERR_INVALID_STATE, TAG, "component begin state");
    const int index = find_plan_component(component);
    ESP_RETURN_ON_FALSE(index >= 0 && !s_update.plan[index].completed,
                        ESP_ERR_INVALID_ARG, TAG,
                        "descriptor not in validated plan");

    if (component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_APPLICATION) {
        s_update.ota_partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota_0");
        ESP_RETURN_ON_FALSE(s_update.ota_partition != NULL,
                            ESP_ERR_NOT_FOUND, TAG, "ota_0 missing");
        const size_t erase_size =
            (component->size + FACTORY_SYSTEM_FLASH_SECTOR_BYTES - 1U) &
            ~(FACTORY_SYSTEM_FLASH_SECTOR_BYTES - 1U);
        ESP_RETURN_ON_ERROR(
            esp_partition_erase_range(s_update.ota_partition, 0, erase_size),
            TAG, "erase ota_0");
    } else {
        uint8_t **destination =
            component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_BOOTLOADER
            ? &s_update.bootloader_image
            : &s_update.partition_table_image;
        free(*destination);
        *destination = heap_caps_malloc(component->size,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_RETURN_ON_FALSE(*destination != NULL, ESP_ERR_NO_MEM, TAG,
                            "stage protected component");
        memset(*destination, 0xff, component->size);
    }
    s_update.active_index = index;
    s_update.received = 0;
    update_status_component(component->id, 0, component->size,
                            ESP_IRIS_SYSTEM_UPDATE_PHASE_RECEIVING);
    return ESP_OK;
}

static esp_err_t write_component(
    const esp_iris_system_update_component_t *component, uint32_t offset,
    const uint8_t *data, size_t size, void *user_ctx)
{
    (void)user_ctx;
    if (component == NULL || data == NULL || size == 0 ||
        s_update.active_index < 0 || offset != s_update.received ||
        component->id !=
            s_update.plan[s_update.active_index].descriptor.id ||
        size > component->size - offset) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = ESP_OK;
    if (component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_APPLICATION) {
        err = esp_partition_write(s_update.ota_partition, offset, data, size);
    } else {
        uint8_t *destination =
            component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_BOOTLOADER
            ? s_update.bootloader_image
            : s_update.partition_table_image;
        if (destination == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        memcpy(destination + offset, data, size);
    }
    if (err == ESP_OK) {
        s_update.received += (uint32_t)size;
        update_status_component(component->id, s_update.received,
                                component->size,
                                ESP_IRIS_SYSTEM_UPDATE_PHASE_RECEIVING);
    }
    return err;
}

static esp_err_t validate_memory_image(const uint8_t *image, size_t size)
{
    ESP_RETURN_ON_FALSE(image != NULL && size >= sizeof(esp_image_header_t),
                        ESP_ERR_IMAGE_INVALID, TAG, "short bootloader image");
    const esp_image_header_t *header = (const esp_image_header_t *)image;
    ESP_RETURN_ON_FALSE(header->magic == ESP_IMAGE_HEADER_MAGIC &&
                            header->segment_count > 0 &&
                            header->segment_count <= ESP_IMAGE_MAX_SEGMENTS &&
                            header->chip_id == ESP_CHIP_ID_ESP32S31,
                        ESP_ERR_IMAGE_INVALID, TAG,
                        "invalid bootloader header");
    size_t offset = sizeof(*header);
    uint8_t checksum = 0xef;
    for (uint8_t segment = 0; segment < header->segment_count; ++segment) {
        ESP_RETURN_ON_FALSE(size - offset >= sizeof(esp_image_segment_header_t),
                            ESP_ERR_IMAGE_INVALID, TAG,
                            "truncated segment header");
        esp_image_segment_header_t segment_header;
        memcpy(&segment_header, image + offset, sizeof(segment_header));
        offset += sizeof(segment_header);
        ESP_RETURN_ON_FALSE((segment_header.data_len & 3U) == 0 &&
                                segment_header.data_len <= size - offset,
                            ESP_ERR_IMAGE_INVALID, TAG,
                            "invalid segment size");
        for (uint32_t i = 0; i < segment_header.data_len; ++i) {
            checksum ^= image[offset + i];
        }
        offset += segment_header.data_len;
    }
    const size_t checksum_end = (offset + 1U + 15U) & ~(size_t)15U;
    ESP_RETURN_ON_FALSE(checksum_end <= size &&
                            image[checksum_end - 1U] == checksum,
                        ESP_ERR_IMAGE_INVALID, TAG,
                        "bootloader checksum mismatch");
    size_t image_end = checksum_end;
    if (header->hash_appended == 1) {
        ESP_RETURN_ON_FALSE(size - image_end >= 32, ESP_ERR_IMAGE_INVALID, TAG,
                            "missing bootloader digest");
        uint8_t digest[32];
        ESP_RETURN_ON_ERROR(hash_memory(image, image_end, digest), TAG,
                            "hash bootloader image");
        ESP_RETURN_ON_FALSE(bytes_equal(digest, image + image_end, 32),
                            ESP_ERR_IMAGE_INVALID, TAG,
                            "bootloader digest mismatch");
        image_end += 32;
    }
    for (size_t i = image_end; i < size; ++i) {
        ESP_RETURN_ON_FALSE(image[i] == 0xff, ESP_ERR_IMAGE_INVALID, TAG,
                            "bootloader padding is not erased");
    }
    return ESP_OK;
}

static const esp_partition_info_t *find_partition_entry(
    const esp_partition_info_t *entries, int count, esp_partition_type_t type,
    esp_partition_subtype_t subtype, const char *label)
{
    for (int i = 0; i < count; ++i) {
        if (entries[i].magic == ESP_PARTITION_MAGIC &&
            entries[i].type == type && entries[i].subtype == subtype &&
            strncmp((const char *)entries[i].label, label,
                    sizeof(entries[i].label)) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

static bool partition_entry_matches(const esp_partition_info_t *entries,
                                    int count,
                                    const esp_partition_t *required)
{
    const esp_partition_info_t *entry = find_partition_entry(
        entries, count, required->type, required->subtype, required->label);
    return entry != NULL && entry->pos.offset == required->address &&
           entry->pos.size == required->size;
}

static esp_err_t require_preserved_partition(
    const esp_partition_info_t *entries, int count, esp_partition_type_t type,
    esp_partition_subtype_t subtype, const char *label)
{
    const esp_partition_t *required =
        esp_partition_find_first(type, subtype, label);
    return required != NULL && partition_entry_matches(entries, count, required)
        ? ESP_OK
        : ESP_ERR_INVALID_VERSION;
}

static esp_err_t require_compatible_ota_partition(
    const esp_partition_info_t *entries, int count)
{
    const esp_partition_t *current = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota_0");
    const esp_partition_info_t *target = find_partition_entry(
        entries, count, ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota_0");
    if (current == NULL || target == NULL ||
        target->pos.offset != current->address) {
        return ESP_ERR_INVALID_VERSION;
    }

    const factory_update_plan_component_t *application = NULL;
    for (size_t i = 0; i < s_update.plan_count; ++i) {
        if (s_update.plan[i].descriptor.kind ==
            ESP_IRIS_SYSTEM_UPDATE_COMPONENT_APPLICATION) {
            application = &s_update.plan[i];
            break;
        }
    }
    if (target->pos.size != current->size && application == NULL) {
        return ESP_ERR_INVALID_VERSION;
    }
    if (application != NULL &&
        application->descriptor.size > target->pos.size) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static esp_err_t validate_partition_table(const uint8_t *image)
{
    int count = 0;
    const esp_partition_info_t *entries =
        (const esp_partition_info_t *)image;
    ESP_RETURN_ON_ERROR(esp_partition_table_verify(entries, true, &count), TAG,
                        "invalid target partition table");
    ESP_RETURN_ON_ERROR(require_preserved_partition(
                            entries, count, ESP_PARTITION_TYPE_APP,
                            ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory"),
                        TAG, "factory partition must be preserved");
    ESP_RETURN_ON_ERROR(require_compatible_ota_partition(entries, count), TAG,
                        "ota_0 offset or target size is incompatible");
    ESP_RETURN_ON_ERROR(require_preserved_partition(
                            entries, count, ESP_PARTITION_TYPE_DATA,
                            ESP_PARTITION_SUBTYPE_DATA_OTA, "otadata"),
                        TAG, "OTA data partition must be preserved");
    ESP_RETURN_ON_ERROR(require_preserved_partition(
                            entries, count, ESP_PARTITION_TYPE_DATA,
                            ESP_PARTITION_SUBTYPE_DATA_PHY, "phy_init"),
                        TAG, "PHY data partition must be preserved");
    ESP_RETURN_ON_ERROR(require_preserved_partition(
                            entries, count, ESP_PARTITION_TYPE_DATA,
                            ESP_PARTITION_SUBTYPE_DATA_NVS, "sysmeta"),
                        TAG, "system metadata partition must be preserved");
    ESP_RETURN_ON_ERROR(require_preserved_partition(
                            entries, count, ESP_PARTITION_TYPE_DATA,
                            ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump"),
                        TAG, "core-dump partition must be preserved");
    ESP_RETURN_ON_ERROR(require_preserved_partition(
                            entries, count, ESP_PARTITION_TYPE_DATA,
                            ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs"),
                        TAG, "application NVS partition must be preserved");
    return ESP_OK;
}

static esp_err_t verify_application_on_flash(
    const esp_iris_system_update_component_t *component)
{
    esp_partition_pos_t position = {
        .offset = component->target_offset,
        .size = s_update.ota_partition->size,
    };
    esp_image_metadata_t metadata = {0};
    return esp_image_verify(ESP_IMAGE_VERIFY, &position, &metadata);
}

static esp_err_t end_component(
    const esp_iris_system_update_component_t *component,
    const uint8_t actual_sha256[ESP_IRIS_SYSTEM_SHA256_BYTES], void *user_ctx)
{
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(component != NULL && actual_sha256 != NULL &&
                            s_update.active_index >= 0 &&
                            s_update.received == component->size,
                        ESP_ERR_INVALID_STATE, TAG, "component end state");
    factory_update_plan_component_t *plan =
        &s_update.plan[s_update.active_index];
    ESP_RETURN_ON_FALSE(component->id == plan->descriptor.id &&
                            bytes_equal(actual_sha256,
                                        plan->descriptor.sha256, 32),
                        ESP_ERR_INVALID_CRC, TAG,
                        "component digest does not match plan");

    esp_err_t ret = ESP_OK;
    if (component->kind == ESP_IRIS_SYSTEM_UPDATE_COMPONENT_APPLICATION) {
        uint8_t readback_sha256[32];
        ESP_GOTO_ON_ERROR(hash_flash_region(component->target_offset,
                                            component->size,
                                            readback_sha256), done, TAG,
                          "read back application");
        ESP_GOTO_ON_FALSE(bytes_equal(readback_sha256, actual_sha256, 32),
                          ESP_ERR_INVALID_CRC, done, TAG,
                          "application readback mismatch");
        ESP_GOTO_ON_ERROR(verify_application_on_flash(component), done, TAG,
                          "application image validation");
        s_update.application_received = true;
    } else if (component->kind ==
               ESP_IRIS_SYSTEM_UPDATE_COMPONENT_BOOTLOADER) {
        ESP_GOTO_ON_ERROR(validate_memory_image(s_update.bootloader_image,
                                                component->size), done, TAG,
                          "bootloader validation");
    } else if (component->kind ==
               ESP_IRIS_SYSTEM_UPDATE_COMPONENT_PARTITION_TABLE) {
        ESP_GOTO_ON_ERROR(
            validate_partition_table(s_update.partition_table_image), done,
            TAG, "partition policy");
    } else {
        ret = ESP_ERR_NOT_SUPPORTED;
        goto done;
    }
    plan->completed = true;
    update_status_component_complete();
done:
    s_update.active_index = -1;
    s_update.received = 0;
    return ret;
}

static esp_err_t persist_result(
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t result)
{
    return factory_system_metadata_store_last_result(operation_id, result);
}

static esp_err_t commit_protected_image(
    const factory_update_plan_component_t *plan, const uint8_t *image)
{
    if (plan == NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(image != NULL && plan->completed,
                        ESP_ERR_INVALID_STATE, TAG,
                        "protected image is incomplete");
    const esp_iris_system_update_component_t *component = &plan->descriptor;
    ESP_RETURN_ON_ERROR(esp_flash_erase_region(NULL, component->target_offset,
                                               component->size),
                        TAG, "erase protected component");
    ESP_RETURN_ON_ERROR(esp_flash_write(NULL, image, component->target_offset,
                                       component->size),
                        TAG, "write protected component");
    uint8_t digest[32];
    ESP_RETURN_ON_ERROR(hash_flash_region(component->target_offset,
                                          component->size, digest),
                        TAG, "read back protected component");
    return bytes_equal(digest, component->sha256, 32)
        ? ESP_OK
        : ESP_ERR_INVALID_CRC;
}

static const factory_update_plan_component_t *plan_for_kind(
    esp_iris_system_update_component_kind_t kind)
{
    for (size_t i = 0; i < s_update.plan_count; ++i) {
        if (s_update.plan[i].descriptor.kind == kind) {
            return &s_update.plan[i];
        }
    }
    return NULL;
}

static void restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(FACTORY_SYSTEM_RESTART_DELAY_MS));
    esp_restart();
}

static esp_err_t commit_update(
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    void *user_ctx)
{
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(operation_id != NULL && s_update.prepared &&
                            s_update.active_index < 0 &&
                            bytes_equal(operation_id, s_update.operation_id,
                                        sizeof(s_update.operation_id)),
                        ESP_ERR_INVALID_STATE, TAG, "commit state");
    update_status_finish(ESP_IRIS_SYSTEM_UPDATE_PHASE_COMMITTING, ESP_OK);
    for (size_t i = 0; i < s_update.plan_count; ++i) {
        ESP_RETURN_ON_FALSE(s_update.plan[i].completed,
                            ESP_ERR_INVALID_STATE, TAG,
                            "component %u incomplete",
                            s_update.plan[i].descriptor.id);
    }

    const factory_update_plan_component_t *bootloader = plan_for_kind(
        ESP_IRIS_SYSTEM_UPDATE_COMPONENT_BOOTLOADER);
    const factory_update_plan_component_t *partition_table = plan_for_kind(
        ESP_IRIS_SYSTEM_UPDATE_COMPONENT_PARTITION_TABLE);

    /* This product deliberately accepts the ESP32-S31 single-copy commit
     * policy: bootloader first, partition table last, with mandatory readback.
     * Recovery and data ranges cannot move. ota_0 keeps its start address but
     * may shrink from the Flash tail when the same plan installs a fitting
     * application image. */
    ESP_RETURN_ON_ERROR(
        commit_protected_image(bootloader, s_update.bootloader_image), TAG,
        "commit bootloader");
    if (bootloader != NULL) {
        uint32_t image_length = 0;
        ESP_RETURN_ON_ERROR(esp_image_verify_bootloader(&image_length), TAG,
                            "verify committed bootloader");
    }
    ESP_RETURN_ON_ERROR(commit_protected_image(
                            partition_table, s_update.partition_table_image),
                        TAG, "commit partition table");
    if (s_update.application_received) {
        ESP_RETURN_ON_ERROR(esp_ota_set_boot_partition(s_update.ota_partition),
                            TAG, "select ota_0");
    }
    ESP_RETURN_ON_ERROR(persist_result(operation_id, ESP_OK), TAG,
                        "persist system update result");
    ESP_RETURN_ON_FALSE(xTaskCreate(restart_task, "factory_restart", 2048,
                                    NULL, 5, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "schedule restart");
    update_status_finish(ESP_IRIS_SYSTEM_UPDATE_PHASE_COMMITTED, ESP_OK);
    ESP_LOGI(TAG, "system update committed; restart scheduled");
    return ESP_OK;
}

static void abort_update_owned(
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t reason, factory_system_update_owner_t owner)
{
    if (!update_owner_is(owner)) {
        ESP_LOGW(TAG, "ignored abort from non-owner %d", (int)owner);
        return;
    }
    if (operation_id == NULL || !s_update.prepared ||
        !bytes_equal(operation_id, s_update.operation_id,
                     ESP_IRIS_SYSTEM_OPERATION_ID_BYTES)) {
        ESP_LOGW(TAG, "ignored abort for inactive operation");
        return;
    }
    const bool prepared = s_update.prepared;
    uint8_t saved_operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES] = {0};
    if (operation_id != NULL) {
        memcpy(saved_operation_id, operation_id, sizeof(saved_operation_id));
    }
    update_state_reset();
    update_status_finish(ESP_IRIS_SYSTEM_UPDATE_PHASE_FAILED, reason);
    update_owner_release();
    if (prepared &&
        persist_result(saved_operation_id, reason) != ESP_OK) {
        ESP_LOGE(TAG, "could not persist failed system-update result");
    }
    ESP_LOGW(TAG, "system update aborted: %s", esp_err_to_name(reason));
}

static void abort_update(
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t reason, void *user_ctx)
{
    (void)user_ctx;
    abort_update_owned(operation_id, reason,
                       FACTORY_SYSTEM_UPDATE_OWNER_ESP_IRIS);
}

esp_err_t factory_system_update_get_status(
    factory_system_update_status_t *status)
{
    ESP_RETURN_ON_FALSE(status != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "status output is null");
    taskENTER_CRITICAL(&s_state_lock);
    *status = s_status;
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

esp_err_t factory_system_update_source_prepare(
    factory_system_update_owner_t owner,
    const uint8_t *manifest, size_t manifest_size,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES])
{
    ESP_RETURN_ON_FALSE(
        manifest != NULL && manifest_size > 0 &&
            manifest_size <= CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES &&
            operation_id != NULL,
        ESP_ERR_INVALID_ARG, TAG, "invalid local update manifest");

    bool nonzero_operation_id = false;
    for (size_t i = 0; i < ESP_IRIS_SYSTEM_OPERATION_ID_BYTES; ++i) {
        nonzero_operation_id |= operation_id[i] != 0;
    }
    ESP_RETURN_ON_FALSE(nonzero_operation_id, ESP_ERR_INVALID_ARG, TAG,
                        "local operation ID is zero");

    cJSON *root = cJSON_ParseWithLength((const char *)manifest,
                                        manifest_size);
    ESP_RETURN_ON_FALSE(root != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "parse local update manifest");
    const cJSON *components = json_member(root, "components");
    const int component_count = cJSON_IsArray(components)
        ? cJSON_GetArraySize(components) : 0;
    cJSON_Delete(root);
    ESP_RETURN_ON_FALSE(
        component_count > 0 &&
            component_count <= CONFIG_ESP_IRIS_SYSTEM_UPDATE_MAX_COMPONENTS,
        ESP_ERR_INVALID_SIZE, TAG, "local component count");

    esp_iris_system_update_manifest_t descriptor = {
        .manifest = manifest,
        .manifest_size = manifest_size,
        .signature = NULL,
        .signature_size = 0,
        .component_count = (uint8_t)component_count,
    };
    memcpy(descriptor.operation_id, operation_id,
           sizeof(descriptor.operation_id));
    ESP_RETURN_ON_ERROR(hash_memory(manifest, manifest_size,
                                    descriptor.manifest_sha256),
                        TAG, "hash local update manifest");
    ESP_RETURN_ON_FALSE(
        owner == FACTORY_SYSTEM_UPDATE_OWNER_HTTP ||
            owner == FACTORY_SYSTEM_UPDATE_OWNER_NAND,
        ESP_ERR_INVALID_ARG, TAG, "invalid system-update source owner");
    return prepare_update_owned(&descriptor, owner);
}

size_t factory_system_update_source_component_count(
    factory_system_update_owner_t owner)
{
    return update_owner_is(owner) && s_update.prepared
        ? s_update.plan_count : 0;
}

esp_err_t factory_system_update_source_component(
    factory_system_update_owner_t owner, size_t index,
    esp_iris_system_update_component_t *component,
    char filename[FACTORY_SYSTEM_UPDATE_FILENAME_BYTES])
{
    ESP_RETURN_ON_FALSE(
        update_owner_is(owner) && s_update.prepared &&
            index < s_update.plan_count &&
            component != NULL && filename != NULL,
        ESP_ERR_INVALID_ARG, TAG, "invalid local component request");
    *component = s_update.plan[index].descriptor;
    strlcpy(filename, s_update.plan[index].filename,
            FACTORY_SYSTEM_UPDATE_FILENAME_BYTES);
    return ESP_OK;
}

esp_err_t factory_system_update_source_begin_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component)
{
    ESP_RETURN_ON_FALSE(update_owner_is(owner),
                        ESP_ERR_INVALID_STATE, TAG,
                        "update source does not own writer");
    return begin_component(component, &s_update);
}

esp_err_t factory_system_update_source_write_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component, uint32_t offset,
    const uint8_t *data, size_t size)
{
    ESP_RETURN_ON_FALSE(update_owner_is(owner),
                        ESP_ERR_INVALID_STATE, TAG,
                        "update source does not own writer");
    return write_component(component, offset, data, size, &s_update);
}

esp_err_t factory_system_update_source_end_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component,
    const uint8_t actual_sha256[ESP_IRIS_SYSTEM_SHA256_BYTES])
{
    ESP_RETURN_ON_FALSE(update_owner_is(owner),
                        ESP_ERR_INVALID_STATE, TAG,
                        "update source does not own writer");
    return end_component(component, actual_sha256, &s_update);
}

esp_err_t factory_system_update_source_commit(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES])
{
    ESP_RETURN_ON_FALSE(update_owner_is(owner),
                        ESP_ERR_INVALID_STATE, TAG,
                        "update source does not own writer");
    return commit_update(operation_id, &s_update);
}

void factory_system_update_source_abort(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t reason)
{
    abort_update_owned(operation_id, reason, owner);
}

esp_err_t factory_system_update_register(void)
{
    const esp_iris_system_update_backend_t backend = {
        .prepare = prepare_update,
        .begin_component = begin_component,
        .write_component = write_component,
        .end_component = end_component,
        .commit = commit_update,
        .abort = abort_update,
        .user_ctx = &s_update,
    };
    ESP_RETURN_ON_ERROR(esp_iris_system_update_register(&backend), TAG,
                        "register system update backend");
    ESP_RETURN_ON_ERROR(factory_system_update_http_register(), TAG,
                        "register HTTP system-update trigger");
    ESP_RETURN_ON_ERROR(factory_system_update_nand_register(), TAG,
                        "register NAND system-update trigger");
    ESP_LOGW(TAG, "unsigned full-system update backend enabled");
    return ESP_OK;
}

#else

esp_err_t factory_system_update_register(void)
{
    return ESP_OK;
}

esp_err_t factory_system_update_get_status(
    factory_system_update_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(status, 0, sizeof(*status));
    status->update.phase = ESP_IRIS_SYSTEM_UPDATE_PHASE_IDLE;
    return ESP_OK;
}

esp_err_t factory_system_update_source_prepare(
    factory_system_update_owner_t owner,
    const uint8_t *manifest, size_t manifest_size,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES])
{
    (void)owner;
    (void)manifest;
    (void)manifest_size;
    (void)operation_id;
    return ESP_ERR_NOT_SUPPORTED;
}

size_t factory_system_update_source_component_count(
    factory_system_update_owner_t owner)
{
    (void)owner;
    return 0;
}

esp_err_t factory_system_update_source_component(
    factory_system_update_owner_t owner, size_t index,
    esp_iris_system_update_component_t *component,
    char filename[FACTORY_SYSTEM_UPDATE_FILENAME_BYTES])
{
    (void)owner;
    (void)index;
    (void)component;
    (void)filename;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_system_update_source_begin_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component)
{
    (void)owner;
    (void)component;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_system_update_source_write_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component, uint32_t offset,
    const uint8_t *data, size_t size)
{
    (void)owner;
    (void)component;
    (void)offset;
    (void)data;
    (void)size;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_system_update_source_end_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component,
    const uint8_t actual_sha256[ESP_IRIS_SYSTEM_SHA256_BYTES])
{
    (void)owner;
    (void)component;
    (void)actual_sha256;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_system_update_source_commit(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES])
{
    (void)owner;
    (void)operation_id;
    return ESP_ERR_NOT_SUPPORTED;
}

void factory_system_update_source_abort(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t reason)
{
    (void)owner;
    (void)operation_id;
    (void)reason;
}

#endif
