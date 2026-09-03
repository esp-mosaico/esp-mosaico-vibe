// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "factory_system_update.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FACTORY_NAND_UPDATE_MAX_CANDIDATES 8U
#define FACTORY_NAND_UPDATE_RELEASE_BYTES 48U

typedef enum {
    FACTORY_NAND_SCAN_IDLE = 0,
    FACTORY_NAND_SCAN_RUNNING,
    FACTORY_NAND_SCAN_READY,
    FACTORY_NAND_SCAN_FAILED,
} factory_nand_scan_state_t;

typedef enum {
    FACTORY_NAND_UPDATE_IDLE = 0,
    FACTORY_NAND_UPDATE_STARTING,
    FACTORY_NAND_UPDATE_RUNNING,
    FACTORY_NAND_UPDATE_FAILED,
} factory_nand_update_state_t;

typedef struct {
    char manifest_path[FACTORY_SYSTEM_UPDATE_PATH_BYTES];
    char release[FACTORY_NAND_UPDATE_RELEASE_BYTES];
    uint64_t total_size;
    uint8_t component_count;
} factory_nand_update_candidate_t;

typedef struct {
    factory_nand_scan_state_t scan_state;
    factory_nand_update_state_t update_state;
    uint32_t generation;
    esp_err_t scan_result;
    esp_err_t update_result;
    size_t candidate_count;
    size_t invalid_count;
    factory_nand_update_candidate_t
        candidates[FACTORY_NAND_UPDATE_MAX_CANDIDATES];
} factory_nand_update_snapshot_t;

/* Scan the bounded Recovery bundle catalog below /nand/system-update. The
 * operation is asynchronous and never formats or writes the NAND filesystem. */
esp_err_t factory_nand_update_request_scan(void);

/* Copy the latest catalog and source status for presentation by the UI. */
esp_err_t factory_nand_update_get_snapshot(
    factory_nand_update_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
