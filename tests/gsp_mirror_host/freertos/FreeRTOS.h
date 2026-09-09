#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdTRUE 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(value) (value)
