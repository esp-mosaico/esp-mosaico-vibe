#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Register read-only system inventory before esp_iris_start(). */
esp_err_t factory_system_inventory_register(void);

#ifdef __cplusplus
}
#endif
