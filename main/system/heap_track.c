#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "heap_track.h"

void heap_track_init(void) {
}

void heap_track_report(void) {
    printf("=== heap tracker ===\n");
    printf("  DRAM free : %lu B\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf("  PSRAM free: %lu B\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("  total free: %lu B\n", (unsigned long)esp_get_free_heap_size());
    printf("  largest   : %lu B\n", (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("  min ever  : %lu B\n", (unsigned long)esp_get_minimum_free_heap_size());
    printf("====================\n");
}

uint32_t heap_track_free(void) {
    return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

uint32_t heap_track_used(void) {
    return (uint32_t)(heap_caps_get_total_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

uint32_t heap_track_largest_free(void) {
    return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}
