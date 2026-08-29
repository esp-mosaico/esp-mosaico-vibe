#pragma once

#include "esp_err.h"

/* Register the recovery-only, product-owned authenticated Flash backend.
 * When the backend is disabled, this remains a successful no-op and the
 * read-only System Inventory service is still available. */
esp_err_t factory_system_update_register(void);
