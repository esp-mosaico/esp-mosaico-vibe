// SPDX-License-Identifier: Apache-2.0

#include "factory_system_update.h"
#include "factory_nand_update.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sdkconfig.h"

#if CONFIG_GET_STARTED_RECOVERY && CONFIG_ESP_IRIS_SYSTEM_UPDATE && \
    CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_BACKEND && \
    CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE

#include "bsp/esp_mosaico.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_blockdev.h"
#include "esp_check.h"
#include "esp_iris.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "spi_nand_flash.h"

#define NAND_UPDATE_SERVICE_ID 0x1201U
#define NAND_UPDATE_START_METHOD_ID 2U
#define NAND_UPDATE_READ_BYTES 4096U
#define NAND_UPDATE_MOUNT_PATH "/nand"
#define NAND_FILE_VOLUME_ID "nand"
#define NAND_UPDATE_CATALOG_PATH NAND_UPDATE_MOUNT_PATH "/system-update"
#define NAND_UPDATE_MANIFEST_NAME "manifest.json"
#define NAND_UPDATE_MAX_DIRECTORY_ENTRIES 64U
#define NAND_UPDATE_LOG_STEP_PERCENT 10U

typedef struct {
    char manifest_path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
} nand_update_task_context_t;

typedef struct {
    esp_blockdev_handle_t blockdev;
    spi_device_handle_t spi;
    bool bus_initialized;
    bool mounted;
} nand_filesystem_t;

static const char *TAG = "factory_nand_update";
static nand_filesystem_t s_nand;
static TaskHandle_t s_nand_task;
static bool s_nand_busy;
static portMUX_TYPE s_nand_task_lock = portMUX_INITIALIZER_UNLOCKED;
static StaticSemaphore_t s_nand_snapshot_mutex_storage;
static SemaphoreHandle_t s_nand_snapshot_mutex;
static factory_nand_update_snapshot_t s_nand_snapshot;

static void snapshot_lock(void)
{
    if (s_nand_snapshot_mutex != NULL) {
        (void)xSemaphoreTake(s_nand_snapshot_mutex, portMAX_DELAY);
    }
}

static void snapshot_unlock(void)
{
    if (s_nand_snapshot_mutex != NULL) {
        (void)xSemaphoreGive(s_nand_snapshot_mutex);
    }
}

static void update_source_status(factory_nand_update_state_t state,
                                 esp_err_t result)
{
    snapshot_lock();
    s_nand_snapshot.update_state = state;
    s_nand_snapshot.update_result = result;
    ++s_nand_snapshot.generation;
    snapshot_unlock();
}

static bool nand_operation_claim(void)
{
    bool claimed = false;
    taskENTER_CRITICAL(&s_nand_task_lock);
    if (!s_nand_busy) {
        s_nand_busy = true;
        claimed = true;
    }
    taskEXIT_CRITICAL(&s_nand_task_lock);
    return claimed;
}

static void nand_operation_release(void)
{
    taskENTER_CRITICAL(&s_nand_task_lock);
    s_nand_task = NULL;
    s_nand_busy = false;
    taskEXIT_CRITICAL(&s_nand_task_lock);
}

static esp_err_t release_control_pin(gpio_num_t pin)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(pin, 1), TAG,
                        "preload NAND control GPIO%d", pin);
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG,
                        "configure NAND control GPIO%d", pin);
    return gpio_set_level(pin, 1);
}

static void nand_cleanup_partial(void)
{
    if (s_nand.blockdev != NULL && !s_nand.mounted) {
        (void)s_nand.blockdev->ops->release(s_nand.blockdev);
        s_nand.blockdev = NULL;
    }
    if (s_nand.spi != NULL) {
        (void)spi_bus_remove_device(s_nand.spi);
        s_nand.spi = NULL;
    }
    if (s_nand.bus_initialized) {
        (void)spi_bus_free(BSP_NAND_SPI_HOST);
        s_nand.bus_initialized = false;
    }
}

static esp_err_t nand_ensure_mounted(void)
{
    if (s_nand.mounted) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(release_control_pin(BSP_NAND_HOLD), TAG,
                        "release NAND HOLD");
    ESP_RETURN_ON_ERROR(release_control_pin(BSP_NAND_WP), TAG,
                        "release NAND WP");

    const spi_bus_config_t bus_config = {
        .mosi_io_num = BSP_NAND_D,
        .miso_io_num = BSP_NAND_Q,
        .sclk_io_num = BSP_NAND_CLK,
        .quadhd_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .max_transfer_sz = NAND_UPDATE_READ_BYTES,
    };
    esp_err_t err = spi_bus_initialize(BSP_NAND_SPI_HOST, &bus_config,
                                       SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }
    s_nand.bus_initialized = true;

    const spi_device_interface_config_t device_config = {
        .clock_speed_hz = BSP_NAND_FLASH_DEFAULT_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = BSP_NAND_CS,
        .queue_size = BSP_NAND_FLASH_DEFAULT_QUEUE_SIZE,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    err = spi_bus_add_device(BSP_NAND_SPI_HOST, &device_config, &s_nand.spi);
    if (err != ESP_OK) {
        nand_cleanup_partial();
        return err;
    }

    spi_nand_flash_config_t nand_config = {
        .device_handle = s_nand.spi,
        .gc_factor = 0,
        .io_mode = SPI_NAND_IO_MODE_SIO,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    err = spi_nand_flash_init_with_layers(&nand_config, &s_nand.blockdev);
    if (err != ESP_OK) {
        nand_cleanup_partial();
        return err;
    }

    const esp_vfs_littlefs_conf_t mount_config = {
        .base_path = NAND_UPDATE_MOUNT_PATH,
        .blockdev = s_nand.blockdev,
        .format_if_mount_failed = false,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false,
    };
    err = esp_vfs_littlefs_register(&mount_config);
    if (err != ESP_OK) {
        /* LittleFS consumes and releases the block device on mount failure. */
        s_nand.blockdev = NULL;
        nand_cleanup_partial();
        return err;
    }
    s_nand.mounted = true;

    size_t total = 0;
    size_t used = 0;
    if (esp_littlefs_blockdev_info(s_nand.blockdev, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "NAND LittleFS mounted read-write: total=%u used=%u",
                 (unsigned)total, (unsigned)used);
    }
    return ESP_OK;
}

static bool manifest_path_valid(const char *path)
{
    if (path == NULL) {
        return false;
    }
    const size_t size = strnlen(path, FACTORY_SYSTEM_UPDATE_PATH_BYTES);
    const size_t prefix_size = strlen(NAND_UPDATE_MOUNT_PATH "/");
    if (size <= prefix_size || size >= FACTORY_SYSTEM_UPDATE_PATH_BYTES ||
        strncmp(path, NAND_UPDATE_MOUNT_PATH "/", prefix_size) != 0 ||
        path[size - 1U] == '/' || strchr(path, '\\') != NULL) {
        return false;
    }
    const char *segment = path + prefix_size;
    while (*segment != '\0') {
        const char *slash = strchr(segment, '/');
        const size_t segment_size = slash != NULL
            ? (size_t)(slash - segment) : strlen(segment);
        if (segment_size == 0 ||
            (segment_size == 1 && segment[0] == '.') ||
            (segment_size == 2 && segment[0] == '.' && segment[1] == '.')) {
            return false;
        }
        if (slash == NULL) {
            break;
        }
        segment = slash + 1;
    }
    return true;
}

static esp_err_t read_manifest(const char *path, uint8_t **manifest,
                               size_t *manifest_size)
{
    struct stat info;
    ESP_RETURN_ON_FALSE(stat(path, &info) == 0 && S_ISREG(info.st_mode),
                        ESP_ERR_NOT_FOUND, TAG, "NAND manifest not found");
    ESP_RETURN_ON_FALSE(
        info.st_size > 0 &&
            (uint64_t)info.st_size <=
                CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES,
        ESP_ERR_INVALID_SIZE, TAG, "NAND manifest size is invalid");

    const size_t size = (size_t)info.st_size;
    uint8_t *data = malloc(size);
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate NAND manifest");
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        free(data);
        return ESP_ERR_NOT_FOUND;
    }
    const size_t received = fread(data, 1, size, file);
    const bool complete = received == size && fgetc(file) == EOF &&
                          !ferror(file);
    fclose(file);
    if (!complete) {
        free(data);
        return ESP_ERR_INVALID_SIZE;
    }
    *manifest = data;
    *manifest_size = size;
    return ESP_OK;
}

static esp_err_t component_path(
    const char *manifest_path, const char *filename,
    char output[FACTORY_SYSTEM_UPDATE_PATH_BYTES])
{
    const char *slash = strrchr(manifest_path, '/');
    ESP_RETURN_ON_FALSE(slash != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "NAND manifest path has no directory");
    const size_t directory_size = (size_t)(slash - manifest_path) + 1U;
    const size_t filename_size = strlen(filename);
    ESP_RETURN_ON_FALSE(
        directory_size + filename_size < FACTORY_SYSTEM_UPDATE_PATH_BYTES,
        ESP_ERR_INVALID_SIZE, TAG, "NAND component path is too long");
    memcpy(output, manifest_path, directory_size);
    memcpy(output + directory_size, filename, filename_size + 1U);
    return ESP_OK;
}

static bool component_filename_valid(const char *filename)
{
    if (filename == NULL) {
        return false;
    }
    const size_t size = strnlen(
        filename, FACTORY_SYSTEM_UPDATE_FILENAME_BYTES);
    return size > 0 && size < FACTORY_SYSTEM_UPDATE_FILENAME_BYTES &&
        strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0 &&
        strchr(filename, '/') == NULL && strchr(filename, '\\') == NULL;
}

static esp_err_t inspect_manifest_candidate(
    const char *manifest_path, factory_nand_update_candidate_t *candidate)
{
    uint8_t *manifest = NULL;
    size_t manifest_size = 0;
    ESP_RETURN_ON_FALSE(candidate != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "candidate output is null");
    ESP_RETURN_ON_ERROR(read_manifest(manifest_path, &manifest,
                                      &manifest_size), TAG,
                        "read NAND catalog manifest");

    cJSON *root = cJSON_ParseWithLength((const char *)manifest,
                                        manifest_size);
    free(manifest);
    ESP_RETURN_ON_FALSE(root != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "parse NAND catalog manifest");

    esp_err_t result = ESP_OK;
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *release = cJSON_GetObjectItemCaseSensitive(root, "release");
    const cJSON *components = cJSON_GetObjectItemCaseSensitive(
        root, "components");
    if (!cJSON_IsString(schema) || schema->valuestring == NULL ||
        strcmp(schema->valuestring, "esp-iris-system-update/v1") != 0 ||
        !cJSON_IsArray(components)) {
        result = ESP_ERR_INVALID_ARG;
        goto done;
    }
    const int component_count = cJSON_GetArraySize(components);
    if (component_count <= 0 ||
        component_count > CONFIG_ESP_IRIS_SYSTEM_UPDATE_MAX_COMPONENTS) {
        result = ESP_ERR_INVALID_SIZE;
        goto done;
    }

    memset(candidate, 0, sizeof(*candidate));
    strlcpy(candidate->manifest_path, manifest_path,
            sizeof(candidate->manifest_path));
    if (cJSON_IsString(release) && release->valuestring != NULL &&
        release->valuestring[0] != '\0') {
        strlcpy(candidate->release, release->valuestring,
                sizeof(candidate->release));
    } else {
        const char *directory_end = strrchr(manifest_path, '/');
        const char *directory_start = directory_end;
        while (directory_start != NULL &&
               directory_start > manifest_path &&
               directory_start[-1] != '/') {
            --directory_start;
        }
        if (directory_start != NULL && directory_end > directory_start) {
            const size_t directory_size = (size_t)(directory_end -
                                                    directory_start);
            const size_t copy_size = directory_size <
                    sizeof(candidate->release) - 1U
                ? directory_size : sizeof(candidate->release) - 1U;
            memcpy(candidate->release, directory_start, copy_size);
            candidate->release[copy_size] = '\0';
        } else {
            strlcpy(candidate->release, "NAND bundle",
                    sizeof(candidate->release));
        }
    }

    for (int i = 0; i < component_count; ++i) {
        const cJSON *item = cJSON_GetArrayItem(components, i);
        if (!cJSON_IsObject(item)) {
            result = ESP_ERR_INVALID_ARG;
            goto done;
        }
        const cJSON *file = cJSON_GetObjectItemCaseSensitive(item, "file");
        const cJSON *size = cJSON_GetObjectItemCaseSensitive(item, "size");
        if (!cJSON_IsString(file) ||
            file->valuestring == NULL ||
            !component_filename_valid(file->valuestring) ||
            !cJSON_IsNumber(size) || size->valuedouble <= 0 ||
            size->valuedouble > UINT32_MAX ||
            (double)(uint32_t)size->valuedouble != size->valuedouble) {
            result = ESP_ERR_INVALID_ARG;
            goto done;
        }
        char path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
        result = component_path(manifest_path, file->valuestring, path);
        if (result != ESP_OK) {
            goto done;
        }
        struct stat info;
        if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
            result = ESP_ERR_NOT_FOUND;
            goto done;
        }
        if ((uint64_t)info.st_size != (uint32_t)size->valuedouble) {
            result = ESP_ERR_INVALID_SIZE;
            goto done;
        }
        candidate->total_size += (uint32_t)size->valuedouble;
    }
    candidate->component_count = (uint8_t)component_count;

done:
    cJSON_Delete(root);
    return result;
}

static int compare_candidates(const void *left, const void *right)
{
    const factory_nand_update_candidate_t *a = left;
    const factory_nand_update_candidate_t *b = right;
    return strcmp(a->manifest_path, b->manifest_path);
}

static void catalog_try_add(factory_nand_update_snapshot_t *snapshot,
                            const char *manifest_path)
{
    struct stat info;
    if (stat(manifest_path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return;
    }
    factory_nand_update_candidate_t candidate;
    if (inspect_manifest_candidate(manifest_path, &candidate) != ESP_OK) {
        ++snapshot->invalid_count;
        return;
    }
    if (snapshot->candidate_count >= FACTORY_NAND_UPDATE_MAX_CANDIDATES) {
        ++snapshot->invalid_count;
        return;
    }
    snapshot->candidates[snapshot->candidate_count++] = candidate;
}

static esp_err_t scan_catalog(factory_nand_update_snapshot_t *snapshot)
{
    struct stat root_info;
    if (stat(NAND_UPDATE_CATALOG_PATH, &root_info) != 0) {
        if (errno == ENOENT) {
            ESP_LOGI(TAG, "NAND system-update catalog is empty");
            return ESP_OK;
        }
        ESP_LOGE(TAG, "stat NAND system-update catalog failed: errno=%d",
                 errno);
        return ESP_FAIL;
    }
    ESP_RETURN_ON_FALSE(S_ISDIR(root_info.st_mode), ESP_ERR_INVALID_STATE, TAG,
                        "NAND system-update catalog is not a directory");

    char manifest_path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
    int written = snprintf(manifest_path, sizeof(manifest_path), "%s/%s",
                           NAND_UPDATE_CATALOG_PATH,
                           NAND_UPDATE_MANIFEST_NAME);
    ESP_RETURN_ON_FALSE(written > 0 &&
                            (size_t)written < sizeof(manifest_path),
                        ESP_ERR_INVALID_SIZE, TAG,
                        "NAND catalog manifest path is too long");
    catalog_try_add(snapshot, manifest_path);

    DIR *directory = opendir(NAND_UPDATE_CATALOG_PATH);
    ESP_RETURN_ON_FALSE(directory != NULL, ESP_FAIL, TAG,
                        "open NAND system-update directory");
    struct dirent *entry;
    size_t entry_count = 0;
    while (entry_count < NAND_UPDATE_MAX_DIRECTORY_ENTRIES &&
           (entry = readdir(directory)) != NULL) {
        ++entry_count;
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            strchr(entry->d_name, '/') != NULL ||
            strchr(entry->d_name, '\\') != NULL) {
            continue;
        }
        char directory_path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
        written = snprintf(directory_path, sizeof(directory_path), "%s/%s",
                           NAND_UPDATE_CATALOG_PATH, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(directory_path)) {
            ++snapshot->invalid_count;
            continue;
        }
        struct stat info;
        if (stat(directory_path, &info) != 0 || !S_ISDIR(info.st_mode)) {
            continue;
        }
        written = snprintf(manifest_path, sizeof(manifest_path), "%s/%s",
                           directory_path, NAND_UPDATE_MANIFEST_NAME);
        if (written <= 0 || (size_t)written >= sizeof(manifest_path)) {
            ++snapshot->invalid_count;
            continue;
        }
        catalog_try_add(snapshot, manifest_path);
    }
    closedir(directory);
    qsort(snapshot->candidates, snapshot->candidate_count,
          sizeof(snapshot->candidates[0]), compare_candidates);
    return ESP_OK;
}

static void catalog_scan_task(void *argument)
{
    (void)argument;
    factory_nand_update_snapshot_t result = {
        .scan_state = FACTORY_NAND_SCAN_READY,
        .scan_result = ESP_OK,
        .update_state = FACTORY_NAND_UPDATE_IDLE,
        .update_result = ESP_OK,
    };
    esp_err_t err = nand_ensure_mounted();
    if (err == ESP_OK) {
        err = scan_catalog(&result);
    }
    if (err != ESP_OK) {
        result.scan_state = FACTORY_NAND_SCAN_FAILED;
        result.scan_result = err;
    }

    snapshot_lock();
    result.generation = s_nand_snapshot.generation + 1U;
    s_nand_snapshot = result;
    snapshot_unlock();
    nand_operation_release();
    vTaskDelete(NULL);
}

static esp_err_t read_component(
    const char *path, const esp_iris_system_update_component_t *component)
{
    struct stat info;
    ESP_RETURN_ON_FALSE(stat(path, &info) == 0 && S_ISREG(info.st_mode),
                        ESP_ERR_NOT_FOUND, TAG, "NAND component not found");
    ESP_RETURN_ON_FALSE((uint64_t)info.st_size == component->size,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "NAND component size mismatch");

    FILE *file = fopen(path, "rb");
    ESP_RETURN_ON_FALSE(file != NULL, ESP_ERR_NOT_FOUND, TAG,
                        "open NAND component");
    esp_err_t err = factory_system_update_source_begin_component(
        FACTORY_SYSTEM_UPDATE_OWNER_NAND, component);
    psa_hash_operation_t hash = PSA_HASH_OPERATION_INIT;
    bool hash_active = false;
    if (err == ESP_OK) {
        err = psa_crypto_init() == PSA_SUCCESS &&
                      psa_hash_setup(&hash, PSA_ALG_SHA_256) == PSA_SUCCESS
            ? ESP_OK : ESP_FAIL;
        hash_active = err == ESP_OK;
    }

    uint8_t *buffer = NULL;
    if (err == ESP_OK) {
        buffer = malloc(NAND_UPDATE_READ_BYTES);
        err = buffer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
    }
    uint32_t received = 0;
    uint32_t next_log_percent = NAND_UPDATE_LOG_STEP_PERCENT;
    while (err == ESP_OK && received < component->size) {
        size_t request = component->size - received;
        if (request > NAND_UPDATE_READ_BYTES) {
            request = NAND_UPDATE_READ_BYTES;
        }
        const size_t read_size = fread(buffer, 1, request, file);
        if (read_size == 0) {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (psa_hash_update(&hash, buffer, read_size) != PSA_SUCCESS) {
            err = ESP_FAIL;
            break;
        }
        err = factory_system_update_source_write_component(
            FACTORY_SYSTEM_UPDATE_OWNER_NAND, component, received, buffer,
            read_size);
        if (err == ESP_OK) {
            received += (uint32_t)read_size;
            const uint32_t percent = (uint32_t)(
                ((uint64_t)received * 100U) / component->size);
            if (received == component->size ||
                percent >= next_log_percent) {
                ESP_LOGI(TAG,
                         "NAND component %u progress: %" PRIu32
                         "/%" PRIu32 " bytes (%" PRIu32 "%%)",
                         component->id, received, component->size, percent);
                next_log_percent =
                    ((percent / NAND_UPDATE_LOG_STEP_PERCENT) + 1U) *
                    NAND_UPDATE_LOG_STEP_PERCENT;
            }
        }
    }

    uint8_t digest[ESP_IRIS_SYSTEM_SHA256_BYTES] = {0};
    size_t digest_size = 0;
    if (err == ESP_OK &&
        (received != component->size || fgetc(file) != EOF || ferror(file))) {
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK) {
        const psa_status_t status = psa_hash_finish(
            &hash, digest, sizeof(digest), &digest_size);
        if (status == PSA_SUCCESS) {
            hash_active = false;
        }
        if (status != PSA_SUCCESS || digest_size != sizeof(digest)) {
            err = ESP_FAIL;
        }
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "finalizing NAND component %u", component->id);
        err = factory_system_update_source_end_component(
            FACTORY_SYSTEM_UPDATE_OWNER_NAND, component, digest);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "NAND component %u verified", component->id);
        }
    }
    if (hash_active) {
        (void)psa_hash_abort(&hash);
    }
    free(buffer);
    fclose(file);
    return err;
}

static void generate_operation_id(
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES])
{
    esp_fill_random(operation_id, ESP_IRIS_SYSTEM_OPERATION_ID_BYTES);
    bool nonzero = false;
    for (size_t i = 0; i < ESP_IRIS_SYSTEM_OPERATION_ID_BYTES; ++i) {
        nonzero |= operation_id[i] != 0;
    }
    if (!nonzero) {
        operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES - 1U] = 1;
    }
}

static void nand_update_task(void *argument)
{
    nand_update_task_context_t *context = argument;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    generate_operation_id(operation_id);
    bool prepared = false;
    uint8_t *manifest = NULL;
    size_t manifest_size = 0;

    esp_err_t err = nand_ensure_mounted();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "reading system-update manifest from NAND");
        err = read_manifest(context->manifest_path, &manifest, &manifest_size);
    }
    if (err == ESP_OK) {
        err = factory_system_update_source_prepare(
            FACTORY_SYSTEM_UPDATE_OWNER_NAND, manifest, manifest_size,
            operation_id);
        prepared = err == ESP_OK;
        if (prepared) {
            update_source_status(FACTORY_NAND_UPDATE_RUNNING, ESP_OK);
        }
    }
    free(manifest);

    const size_t component_count = prepared
        ? factory_system_update_source_component_count(
              FACTORY_SYSTEM_UPDATE_OWNER_NAND)
        : 0;
    for (size_t i = 0; err == ESP_OK && i < component_count; ++i) {
        esp_iris_system_update_component_t component;
        char filename[FACTORY_SYSTEM_UPDATE_FILENAME_BYTES];
        char path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
        err = factory_system_update_source_component(
            FACTORY_SYSTEM_UPDATE_OWNER_NAND, i, &component, filename);
        if (err == ESP_OK) {
            err = component_path(context->manifest_path, filename, path);
        }
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "reading NAND component %u (%lu bytes)",
                     component.id, (unsigned long)component.size);
            err = read_component(path, &component);
        }
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "all NAND components verified; starting commit");
        err = factory_system_update_source_commit(
            FACTORY_SYSTEM_UPDATE_OWNER_NAND, operation_id);
    }
    if (err != ESP_OK) {
        if (prepared) {
            factory_system_update_source_abort(
                FACTORY_SYSTEM_UPDATE_OWNER_NAND, operation_id, err);
        }
        update_source_status(FACTORY_NAND_UPDATE_FAILED, err);
        ESP_LOGE(TAG, "NAND system update failed: %s", esp_err_to_name(err));
    }

    free(context);
    nand_operation_release();
    vTaskDelete(NULL);
}

esp_err_t factory_system_update_start_nand(const char *manifest_path)
{
    ESP_RETURN_ON_FALSE(manifest_path_valid(manifest_path),
                        ESP_ERR_INVALID_ARG, TAG,
                        "invalid NAND system-update manifest path");
    nand_update_task_context_t *context = calloc(1, sizeof(*context));
    ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate NAND update task");
    strlcpy(context->manifest_path, manifest_path,
            sizeof(context->manifest_path));

    if (!nand_operation_claim()) {
        free(context);
        return ESP_ERR_INVALID_STATE;
    }
    update_source_status(FACTORY_NAND_UPDATE_STARTING, ESP_OK);
    if (xTaskCreate(nand_update_task, "nand_sysupdate",
                    CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE_TASK_STACK,
                    context, 4, &s_nand_task) != pdPASS) {
        nand_operation_release();
        update_source_status(FACTORY_NAND_UPDATE_FAILED, ESP_ERR_NO_MEM);
        free(context);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t factory_nand_update_request_scan(void)
{
    ESP_RETURN_ON_FALSE(s_nand_snapshot_mutex != NULL,
                        ESP_ERR_INVALID_STATE, TAG,
                        "NAND catalog is not initialized");
    ESP_RETURN_ON_FALSE(nand_operation_claim(), ESP_ERR_INVALID_STATE, TAG,
                        "NAND update source is busy");

    snapshot_lock();
    s_nand_snapshot.scan_state = FACTORY_NAND_SCAN_RUNNING;
    s_nand_snapshot.scan_result = ESP_OK;
    s_nand_snapshot.update_state = FACTORY_NAND_UPDATE_IDLE;
    s_nand_snapshot.update_result = ESP_OK;
    s_nand_snapshot.candidate_count = 0;
    s_nand_snapshot.invalid_count = 0;
    memset(s_nand_snapshot.candidates, 0,
           sizeof(s_nand_snapshot.candidates));
    ++s_nand_snapshot.generation;
    snapshot_unlock();

    if (xTaskCreate(catalog_scan_task, "nand_catalog",
                    CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE_TASK_STACK,
                    NULL, 3, &s_nand_task) != pdPASS) {
        snapshot_lock();
        s_nand_snapshot.scan_state = FACTORY_NAND_SCAN_FAILED;
        s_nand_snapshot.scan_result = ESP_ERR_NO_MEM;
        ++s_nand_snapshot.generation;
        snapshot_unlock();
        nand_operation_release();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t factory_nand_update_get_snapshot(
    factory_nand_update_snapshot_t *snapshot)
{
    ESP_RETURN_ON_FALSE(snapshot != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "NAND snapshot output is null");
    ESP_RETURN_ON_FALSE(s_nand_snapshot_mutex != NULL,
                        ESP_ERR_INVALID_STATE, TAG,
                        "NAND catalog is not initialized");
    if (xSemaphoreTake(s_nand_snapshot_mutex, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *snapshot = s_nand_snapshot;
    (void)xSemaphoreGive(s_nand_snapshot_mutex);
    return ESP_OK;
}

static esp_err_t nand_update_rpc(const esp_iris_rpc_request_t *request,
                                 uint8_t *response,
                                 size_t response_capacity,
                                 size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(request != NULL && response_size != NULL &&
                            request->payload != NULL &&
                            request->payload_size > 0 &&
                            request->payload_size <
                                FACTORY_SYSTEM_UPDATE_PATH_BYTES,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "invalid NAND update RPC");
    char path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
    memcpy(path, request->payload, request->payload_size);
    path[request->payload_size] = '\0';
    ESP_RETURN_ON_FALSE(memchr(path, '\0', request->payload_size) == NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "NAND update path contains NUL");
    ESP_RETURN_ON_ERROR(factory_system_update_start_nand(path), TAG,
                        "start NAND update");
    *response_size = 0;
    return ESP_OK;
}

esp_err_t factory_system_update_nand_register(void)
{
    if (s_nand_snapshot_mutex == NULL) {
        s_nand_snapshot_mutex = xSemaphoreCreateMutexStatic(
            &s_nand_snapshot_mutex_storage);
    }
    ESP_RETURN_ON_FALSE(s_nand_snapshot_mutex != NULL, ESP_ERR_NO_MEM, TAG,
                        "create NAND catalog mutex");

    /* Mount and publish the volume before esp_iris_start(), which snapshots
     * the registered file volumes into the device capability handshake. NAND
     * remains optional for Recovery availability: a media/mount failure must
     * not take down the USB maintenance path. */
    esp_err_t file_err = nand_ensure_mounted();
    if (file_err == ESP_OK) {
        const esp_iris_file_volume_config_t volume = {
            .id = NAND_FILE_VOLUME_ID,
            .base_path = NAND_UPDATE_MOUNT_PATH,
            .capabilities = ESP_IRIS_FILE_VOLUME_READ |
                            ESP_IRIS_FILE_VOLUME_LIST |
                            ESP_IRIS_FILE_VOLUME_MTIME |
                            ESP_IRIS_FILE_VOLUME_WRITE |
                            ESP_IRIS_FILE_VOLUME_DELETE |
                            ESP_IRIS_FILE_VOLUME_MKDIR |
                            ESP_IRIS_FILE_VOLUME_RENAME |
                            ESP_IRIS_FILE_VOLUME_ATOMIC_REPLACE,
        };
        file_err = esp_iris_file_volume_register(&volume);
    }
    if (file_err == ESP_OK) {
        ESP_LOGI(TAG, "registered writable ESP-Iris volume id=%s base=%s",
                 NAND_FILE_VOLUME_ID, NAND_UPDATE_MOUNT_PATH);
    } else {
        ESP_LOGE(TAG, "NAND file volume unavailable: %s",
                 esp_err_to_name(file_err));
    }

    return esp_iris_rpc_register(NAND_UPDATE_SERVICE_ID,
                                 NAND_UPDATE_START_METHOD_ID,
                                 nand_update_rpc, NULL);
}

#else

esp_err_t factory_system_update_start_nand(const char *manifest_path)
{
    (void)manifest_path;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_system_update_nand_register(void)
{
    return ESP_OK;
}

esp_err_t factory_nand_update_request_scan(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_nand_update_get_snapshot(
    factory_nand_update_snapshot_t *snapshot)
{
    if (snapshot != NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
    }
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
