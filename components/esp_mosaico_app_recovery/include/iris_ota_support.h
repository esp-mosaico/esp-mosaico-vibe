#pragma once

#include "esp_err.h"

/* Start ESP-Iris, expose the enter-Recovery RPC, and enable application
 * health reporting for the retained Recovery workflow. */
void iris_ota_support_start(void);

/* After iris_ota_support_start(), schedule a switch from the normal app to
 * the retained factory Recovery image. ESP_OK means the reboot was scheduled;
 * verify Recovery is ready after the device reconnects. */
esp_err_t iris_ota_support_enter_recovery(void);
