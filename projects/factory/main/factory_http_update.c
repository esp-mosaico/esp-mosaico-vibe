#include "factory_system_update.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#if CONFIG_GET_STARTED_RECOVERY && CONFIG_ESP_IRIS_SYSTEM_UPDATE && \
    CONFIG_IRIS_FACTORY_SYSTEM_UPDATE_BACKEND && \
    CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_random.h"
#include "factory_network.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"

#define HTTP_UPDATE_SERVICE_ID 0x1201U
#define HTTP_UPDATE_START_METHOD_ID 1U
#define HTTP_UPDATE_READ_BYTES 4096U

typedef struct {
    char manifest_url[FACTORY_SYSTEM_UPDATE_URL_BYTES];
} http_update_task_context_t;

static const char *TAG = "factory_http_update";
static TaskHandle_t s_http_task;
static bool s_http_busy;
static portMUX_TYPE s_http_task_lock = portMUX_INITIALIZER_UNLOCKED;

static bool url_is_https(const char *url)
{
    return strncmp(url, "https://", strlen("https://")) == 0;
}

static bool url_is_allowed(const char *url)
{
    if (url_is_https(url)) {
        return true;
    }
#if CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_ALLOW_PLAIN_HTTP
    return strncmp(url, "http://", strlen("http://")) == 0;
#else
    return false;
#endif
}

static esp_http_client_handle_t http_open(const char *url,
                                          int64_t *content_length)
{
    const esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_TIMEOUT_MS,
        .disable_auto_redirect = true,
        .max_redirection_count = 0,
        .max_authorization_retries = 0,
        .buffer_size = HTTP_UPDATE_READ_BYTES,
        .crt_bundle_attach = url_is_https(url) ? esp_crt_bundle_attach : NULL,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return NULL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    int64_t length = -1;
    if (err == ESP_OK) {
        length = esp_http_client_fetch_headers(client);
        if (length < 0) {
            err = (esp_err_t)-length;
        }
    }
    if (err == ESP_OK && esp_http_client_get_status_code(client) != 200) {
        err = ESP_ERR_HTTP_BASE;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        (void)esp_http_client_close(client);
        (void)esp_http_client_cleanup(client);
        return NULL;
    }
    *content_length = length;
    return client;
}

static void http_close(esp_http_client_handle_t client)
{
    if (client != NULL) {
        (void)esp_http_client_close(client);
        (void)esp_http_client_cleanup(client);
    }
}

static esp_err_t download_manifest(const char *url, uint8_t **manifest,
                                   size_t *manifest_size)
{
    int64_t declared_size = -1;
    esp_http_client_handle_t client = http_open(url, &declared_size);
    ESP_RETURN_ON_FALSE(client != NULL, ESP_FAIL, TAG,
                        "open update manifest");
    if (declared_size > CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES) {
        http_close(client);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *data = malloc(CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES + 1U);
    if (data == NULL) {
        http_close(client);
        return ESP_ERR_NO_MEM;
    }
    size_t received = 0;
    esp_err_t err = ESP_OK;
    while (received <= CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES) {
        const size_t available =
            CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES + 1U - received;
        const int read_size = esp_http_client_read(
            client, (char *)data + received, (int)available);
        if (read_size == 0) {
            break;
        }
        if (read_size < 0) {
            err = read_size == -ESP_ERR_HTTP_EAGAIN
                ? ESP_ERR_TIMEOUT : ESP_FAIL;
            break;
        }
        received += (size_t)read_size;
    }
    if (err == ESP_OK &&
        (!esp_http_client_is_complete_data_received(client) || received == 0 ||
         received > CONFIG_ESP_IRIS_SYSTEM_UPDATE_MANIFEST_BYTES ||
         (declared_size > 0 && received != (size_t)declared_size))) {
        err = ESP_ERR_INVALID_SIZE;
    }
    http_close(client);
    if (err != ESP_OK) {
        free(data);
        return err;
    }
    data[received] = '\0';
    *manifest = data;
    *manifest_size = received;
    return ESP_OK;
}

static esp_err_t component_url(const char *manifest_url,
                               const char *filename,
                               char output[FACTORY_SYSTEM_UPDATE_URL_BYTES])
{
    static const char hex[] = "0123456789ABCDEF";
    const char *slash = strrchr(manifest_url, '/');
    ESP_RETURN_ON_FALSE(slash != NULL && slash[1] != '\0',
                        ESP_ERR_INVALID_ARG, TAG,
                        "manifest URL has no filename");
    const size_t directory_size = (size_t)(slash - manifest_url) + 1U;
    memcpy(output, manifest_url, directory_size);
    size_t output_size = directory_size;
    for (const unsigned char *cursor = (const unsigned char *)filename;
         *cursor != '\0'; ++cursor) {
        const bool unreserved =
            (*cursor >= 'a' && *cursor <= 'z') ||
            (*cursor >= 'A' && *cursor <= 'Z') ||
            (*cursor >= '0' && *cursor <= '9') ||
            *cursor == '-' || *cursor == '.' || *cursor == '_' ||
            *cursor == '~';
        const size_t encoded_size = unreserved ? 1U : 3U;
        ESP_RETURN_ON_FALSE(
            output_size + encoded_size < FACTORY_SYSTEM_UPDATE_URL_BYTES,
            ESP_ERR_INVALID_SIZE, TAG, "component URL is too long");
        if (unreserved) {
            output[output_size++] = (char)*cursor;
        } else {
            output[output_size++] = '%';
            output[output_size++] = hex[*cursor >> 4U];
            output[output_size++] = hex[*cursor & 0x0fU];
        }
    }
    output[output_size] = '\0';
    return ESP_OK;
}

static esp_err_t download_component(
    const char *url, const esp_iris_system_update_component_t *component)
{
    int64_t declared_size = -1;
    esp_http_client_handle_t client = http_open(url, &declared_size);
    ESP_RETURN_ON_FALSE(client != NULL, ESP_FAIL, TAG,
                        "open system component");
    if (declared_size > 0 && declared_size != component->size) {
        http_close(client);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = factory_system_update_local_begin_component(component);
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
        buffer = malloc(HTTP_UPDATE_READ_BYTES);
        err = buffer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
    }
    uint32_t received = 0;
    while (err == ESP_OK && received < component->size) {
        size_t request = component->size - received;
        if (request > HTTP_UPDATE_READ_BYTES) {
            request = HTTP_UPDATE_READ_BYTES;
        }
        const int read_size = esp_http_client_read(
            client, (char *)buffer, (int)request);
        if (read_size <= 0) {
            err = read_size == -ESP_ERR_HTTP_EAGAIN
                ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_SIZE;
            break;
        }
        if (psa_hash_update(&hash, buffer, (size_t)read_size) != PSA_SUCCESS) {
            err = ESP_FAIL;
            break;
        }
        err = factory_system_update_local_write_component(
            component, received, buffer, (size_t)read_size);
        if (err == ESP_OK) {
            received += (uint32_t)read_size;
        }
    }

    uint8_t digest[ESP_IRIS_SYSTEM_SHA256_BYTES] = {0};
    size_t digest_size = 0;
    if (err == ESP_OK && received == component->size &&
        !esp_http_client_is_complete_data_received(client)) {
        const int trailing = esp_http_client_read(client, (char *)buffer, 1);
        if (trailing == -ESP_ERR_HTTP_EAGAIN) {
            err = ESP_ERR_TIMEOUT;
        } else if (trailing != 0 ||
                   !esp_http_client_is_complete_data_received(client)) {
            err = ESP_ERR_INVALID_SIZE;
        }
    }
    if (err == ESP_OK &&
        (!esp_http_client_is_complete_data_received(client) ||
         received != component->size)) {
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK) {
        const psa_status_t hash_status = psa_hash_finish(
            &hash, digest, sizeof(digest), &digest_size);
        if (hash_status == PSA_SUCCESS) {
            hash_active = false;
        }
        if (hash_status != PSA_SUCCESS || digest_size != sizeof(digest)) {
            err = ESP_FAIL;
        }
    }
    if (err == ESP_OK) {
        err = factory_system_update_local_end_component(component, digest);
    }
    if (hash_active) {
        (void)psa_hash_abort(&hash);
    }
    free(buffer);
    http_close(client);
    return err;
}

static esp_err_t wait_for_network(void)
{
    const TickType_t delay = pdMS_TO_TICKS(250);
    const TickType_t deadline = xTaskGetTickCount() +
        pdMS_TO_TICKS(CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_NETWORK_WAIT_MS);
    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        factory_network_snapshot_t network = {0};
        if (factory_network_get_snapshot(&network) == ESP_OK &&
            network.state == FACTORY_NETWORK_CONNECTED) {
            return ESP_OK;
        }
        vTaskDelay(delay);
    }
    return ESP_ERR_TIMEOUT;
}

static void http_update_task(void *argument)
{
    http_update_task_context_t *context = argument;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    esp_fill_random(operation_id, sizeof(operation_id));
    bool prepared = false;
    uint8_t *manifest = NULL;
    size_t manifest_size = 0;

    esp_err_t err = wait_for_network();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "downloading system-update manifest");
        err = download_manifest(context->manifest_url, &manifest,
                                &manifest_size);
    }
    if (err == ESP_OK) {
        err = factory_system_update_local_prepare(
            manifest, manifest_size, operation_id);
        prepared = err == ESP_OK;
    }
    free(manifest);

    const size_t component_count = prepared
        ? factory_system_update_local_component_count() : 0;
    for (size_t i = 0; err == ESP_OK && i < component_count; ++i) {
        esp_iris_system_update_component_t component;
        char filename[FACTORY_SYSTEM_UPDATE_FILENAME_BYTES];
        char url[FACTORY_SYSTEM_UPDATE_URL_BYTES];
        err = factory_system_update_local_component(i, &component, filename);
        if (err == ESP_OK) {
            err = component_url(context->manifest_url, filename, url);
        }
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "downloading component %u (%lu bytes)",
                     component.id, (unsigned long)component.size);
            err = download_component(url, &component);
        }
    }
    if (err == ESP_OK) {
        err = factory_system_update_local_commit(operation_id);
    }
    if (err != ESP_OK) {
        if (prepared) {
            factory_system_update_local_abort(operation_id, err);
        }
        ESP_LOGE(TAG, "HTTP system update failed: %s", esp_err_to_name(err));
    }

    free(context);
    taskENTER_CRITICAL(&s_http_task_lock);
    s_http_task = NULL;
    s_http_busy = false;
    taskEXIT_CRITICAL(&s_http_task_lock);
    vTaskDelete(NULL);
}

esp_err_t factory_system_update_start_http(const char *manifest_url)
{
    ESP_RETURN_ON_FALSE(manifest_url != NULL && url_is_allowed(manifest_url),
                        ESP_ERR_INVALID_ARG, TAG,
                        "system-update URL scheme is not allowed");
    const size_t url_size = strnlen(manifest_url,
                                    FACTORY_SYSTEM_UPDATE_URL_BYTES);
    ESP_RETURN_ON_FALSE(url_size > 0 &&
                            url_size < FACTORY_SYSTEM_UPDATE_URL_BYTES,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "system-update URL is too long");

    http_update_task_context_t *context = calloc(1, sizeof(*context));
    ESP_RETURN_ON_FALSE(context != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate HTTP update task");
    strlcpy(context->manifest_url, manifest_url,
            sizeof(context->manifest_url));

    taskENTER_CRITICAL(&s_http_task_lock);
    const bool busy = s_http_busy;
    if (!busy) {
        s_http_busy = true;
    }
    taskEXIT_CRITICAL(&s_http_task_lock);
    if (busy) {
        free(context);
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskCreate(http_update_task, "http_sysupdate",
                    CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_TASK_STACK,
                    context, 4, &s_http_task) != pdPASS) {
        taskENTER_CRITICAL(&s_http_task_lock);
        s_http_busy = false;
        s_http_task = NULL;
        taskEXIT_CRITICAL(&s_http_task_lock);
        free(context);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t http_update_rpc(const esp_iris_rpc_request_t *request,
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
                                FACTORY_SYSTEM_UPDATE_URL_BYTES,
                        ESP_ERR_INVALID_SIZE, TAG, "invalid HTTP update RPC");
    char url[FACTORY_SYSTEM_UPDATE_URL_BYTES];
    memcpy(url, request->payload, request->payload_size);
    url[request->payload_size] = '\0';
    ESP_RETURN_ON_FALSE(memchr(url, '\0', request->payload_size) == NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "HTTP update URL contains NUL");
    ESP_RETURN_ON_ERROR(factory_system_update_start_http(url), TAG,
                        "start HTTP update");
    *response_size = 0;
    return ESP_OK;
}

esp_err_t factory_system_update_http_register(void)
{
    return esp_iris_rpc_register(HTTP_UPDATE_SERVICE_ID,
                                 HTTP_UPDATE_START_METHOD_ID,
                                 http_update_rpc, NULL);
}

#else

esp_err_t factory_system_update_start_http(const char *manifest_url)
{
    (void)manifest_url;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t factory_system_update_http_register(void)
{
    return ESP_OK;
}

#endif
