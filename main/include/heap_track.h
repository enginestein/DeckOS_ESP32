#pragma once
#include <stdint.h>

void     heap_track_init(void);
void     heap_track_report(void);
uint32_t heap_track_free(void);
uint32_t heap_track_used(void);
uint32_t heap_track_largest_free(void);
