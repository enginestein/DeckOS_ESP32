#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kernel.h"
#include "shell.h"
#include "commands.h"
#include "drivers.h"
#include "scheduler.h"
#include "bootloader.h"
#include "syslog.h"
#include "bt.h"
#include "vfs.h"
#include "module.h"
#include "file_persist.h"
#include "hal.h"

// Pending commands queue (cross-task)
static char pending_cmds[MAX_PENDING_CMDS][INPUT_SIZE];
static int pending_head = 0;
static int pending_tail = 0;

void pending_commands_poll(void) {
    if (pending_head == pending_tail) return;
    char tmp[INPUT_SIZE];
    strncpy(tmp, pending_cmds[pending_head], INPUT_SIZE - 1);
    tmp[INPUT_SIZE - 1] = '\0';
    pending_head = (pending_head + 1) % MAX_PENDING_CMDS;
    printf("exec: '%s'\n", tmp);
    commands_execute(tmp);
}

void kernel_enqueue_command(const char* cmd) {
    int next = (pending_tail + 1) % MAX_PENDING_CMDS;
    if (next != pending_head) {
        strncpy(pending_cmds[pending_tail], cmd, INPUT_SIZE - 1);
        pending_cmds[pending_tail][INPUT_SIZE - 1] = '\0';
        pending_tail = next;
    } else {
        printf("cron: command queue full\n");
    }
}

void kernel_poll(void) {
    cron_poll();
    pending_commands_poll();
    shell_run();
}

void kernel_init(void) {
    syslog_init();
    LOG_I("kernel", "booting DeckOS ESP32");

    bootloader_run();
    vfs_load();
    drivers_init_all();

    LOG_I("kernel", "drivers ready");
    sched_init();
    LOG_I("kernel", "scheduler ready");

    modules_init();
    LOG_I("kernel", "modules registered");

    printf("[kernel] initialized\n");
    shell_init();
    LOG_I("kernel", "shell ready");
}

void kernel_run(void) {
    while (true) {
        cron_poll();
        pending_commands_poll();
        shell_run();
        hal_sleep_ms(1);
    }
}
