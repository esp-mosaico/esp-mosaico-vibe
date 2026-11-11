#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"
#include "esp_err.h"

esp_err_t minihub_zigbee_permit_join(int seconds);
bool minihub_zigbee_permit_join_active(void);
bool minihub_zigbee_running(void);
void minihub_zigbee_json_add_permit_join_error(cJSON *obj, esp_err_t err);
esp_err_t minihub_zigbee_add_install_code(uint64_t ieee, const char *ic_hex);
esp_err_t minihub_zigbee_remove_device(const char *point_id);
