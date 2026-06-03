#pragma once
#include <stdbool.h>

#define MAX_ARGS   32
#define MAX_CRON_JOBS 8

typedef struct {
    const char* name;
    void (*handler)(int argc, char* argv[]);
    const char* description;
} command_t;

void commands_init(void);
bool commands_execute(const char* input);
void commands_list(void);
void cron_poll(void);
