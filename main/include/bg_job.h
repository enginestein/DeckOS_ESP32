#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_BG_JOBS 8

typedef enum { BG_IDLE, BG_RUNNING, BG_DONE, BG_CANCELLED } bg_state_t;

typedef struct {
    uint32_t id;
    bg_state_t state;
    uint32_t elapsed_ms;
    void (*fn)(void*);
    void* arg;
} bg_job_t;

void   bg_job_init(void);
int    bg_job_start(void (*fn)(void*), void* arg, uint32_t timeout_ms);
void   bg_job_tick(void);
bool   bg_job_cancel(int id);
void   bg_job_list(void);
