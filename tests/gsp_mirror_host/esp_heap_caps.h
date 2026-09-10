#pragma once

#include <stddef.h>

#define MALLOC_CAP_8BIT 1U
#define MALLOC_CAP_SPIRAM 2U

void *heap_caps_calloc(size_t count, size_t size, unsigned caps);
void *heap_caps_malloc(size_t size, unsigned caps);
void heap_caps_free(void *allocation);
