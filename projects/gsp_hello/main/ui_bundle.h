#pragma once

#include "esp_err.h"
#include "esp_gsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Open and validate the deployable GSP bundle stored in ui_apps. */
esp_err_t ui_bundle_open(esp_gsp_config_t *out_config);

#ifdef __cplusplus
}
#endif
