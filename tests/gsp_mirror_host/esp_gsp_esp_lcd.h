#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_gsp.h"

typedef struct esp_gsp_esp_lcd_pause esp_gsp_esp_lcd_pause_t;

esp_err_t esp_gsp_esp_lcd_pause(esp_gsp_handle_t handle, uint32_t timeout_ms,
                                esp_gsp_esp_lcd_pause_t **pause);
esp_err_t esp_gsp_esp_lcd_resume_paused(esp_gsp_esp_lcd_pause_t *pause,
                                        esp_gsp_handle_t *handle);
