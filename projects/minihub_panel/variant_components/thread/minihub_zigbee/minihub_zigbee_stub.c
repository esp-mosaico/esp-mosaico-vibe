#include "minihub_zigbee.h"

esp_err_t minihub_zigbee_permit_join(int seconds)
{
    (void)seconds;
    return ESP_ERR_NOT_SUPPORTED;
}

bool minihub_zigbee_permit_join_active(void)
{
    return false;
}

bool minihub_zigbee_running(void)
{
    return false;
}

void minihub_zigbee_json_add_permit_join_error(cJSON *obj, esp_err_t err)
{
    if (obj == NULL) {
        return;
    }
    cJSON_AddStringToObject(obj, "error", esp_err_to_name(err));
    cJSON_AddStringToObject(obj, "hint",
                            "Zigbee is unavailable in the Thread firmware");
}

esp_err_t minihub_zigbee_add_install_code(uint64_t ieee, const char *ic_hex)
{
    (void)ieee;
    (void)ic_hex;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t minihub_zigbee_remove_device(const char *point_id)
{
    (void)point_id;
    return ESP_ERR_NOT_SUPPORTED;
}
