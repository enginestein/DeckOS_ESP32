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
#include "esp_netif.h"
#include "esp_event.h"

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

static char s_cron_cmd[256];
static uint64_t s_cron_target_us = 0;
static bool s_cron_active = false;

void cron_schedule(const char *cmd, uint32_t delay_ms) {
    strncpy(s_cron_cmd, cmd, sizeof(s_cron_cmd)-1);
    s_cron_cmd[sizeof(s_cron_cmd)-1] = '\0';
    s_cron_target_us = hal_time_us() + delay_ms * 1000;
    s_cron_active = true;
}

void cron_poll(void) {
    if (!s_cron_active) return;
    if (hal_time_us() >= s_cron_target_us) {
        s_cron_active = false;
        commands_execute(s_cron_cmd);
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

    /* One-time ESP32 net layer init (required before WiFi/Swarm/ESP-NOW) */
    esp_netif_init();
    esp_event_loop_create_default();
    LOG_I("kernel", "net layer ready");

    modules_init();
    LOG_I("kernel", "modules registered");

    printf("[kernel] initialized\n");
    shell_init();
    LOG_I("kernel", "shell ready");
}

void kernel_run(void) {
    static uint64_t last_tick = 0;
    while (true) {
        cron_poll();
        pending_commands_poll();
        shell_run();
        uint64_t now = hal_time_us();
        if (now - last_tick >= 1000000) {
            module_fire_event(MODULE_EVENT_TICK, NULL);
            last_tick = now;
        }
        hal_sleep_ms(1);
    }
}
