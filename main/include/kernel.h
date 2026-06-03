#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_PENDING_CMDS 16
#define INPUT_SIZE 256

void kernel_init(void);
void kernel_run(void);
void kernel_poll(void);
void kernel_enqueue_command(const char* cmd);
void pending_commands_poll(void);
void cron_poll(void);
