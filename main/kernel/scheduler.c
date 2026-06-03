#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scheduler.h"
#include "bg_job.h"
#include "servo.h"
#include "hal.h"

static sched_task_t tasks[SCHED_MAX_TASKS];
static int task_count = 0;
static uint64_t task_total_us[SCHED_MAX_TASKS];
static uint64_t core1_total_us = 0;

static TaskHandle_t sched_task_handle = NULL;

static void core1_entry(void* param) {
    (void)param;
    while (1) {
        uint32_t now = hal_time_ms();

        uint32_t saved = hal_irq_disable();
        int count_snap = task_count;
        hal_irq_restore(saved);

        for (int i = 0; i < count_snap; i++) {
            saved = hal_irq_disable();
            sched_task_t snap = tasks[i];
            hal_irq_restore(saved);

            if (!snap.enabled) continue;
            if ((now - snap.last_run_ms) < snap.interval_ms) continue;

            uint64_t t0 = hal_time_us();
            snap.fn();
            uint64_t elapsed = hal_time_us() - t0;

            saved = hal_irq_disable();
            tasks[i].last_run_ms = now;
            hal_irq_restore(saved);

            task_total_us[i] += elapsed;
            core1_total_us   += elapsed;
        }

        servo_bg_tick();
        bg_job_tick();
        hal_sleep_us(1000);
    }
}

void sched_init(void) {
    memset(tasks,         0, sizeof(tasks));
    memset(task_total_us, 0, sizeof(task_total_us));

    xTaskCreatePinnedToCore(
        core1_entry,
        "sched",
        4096,
        NULL,
        10,
        &sched_task_handle,
        1  // Core 1
    );
    printf("[sched] FreeRTOS scheduler task running on Core 1\n");
}

int sched_register(const char* name, task_fn_t fn, uint32_t interval_ms) {
    uint32_t saved = hal_irq_disable();
    if (task_count >= SCHED_MAX_TASKS || !fn) {
        hal_irq_restore(saved);
        return -1;
    }
    int id = task_count;
    tasks[id].name        = name;
    tasks[id].fn          = fn;
    tasks[id].interval_ms = interval_ms ? interval_ms : 1;
    tasks[id].last_run_ms = 0;
    tasks[id].enabled     = true;
    task_count++;
    hal_irq_restore(saved);
    return id;
}

void sched_enable(int id, bool enable) {
    uint32_t saved = hal_irq_disable();
    if (id >= 0 && id < task_count)
        tasks[id].enabled = enable;
    hal_irq_restore(saved);
}

void sched_list(void) {
    uint32_t saved = hal_irq_disable();
    int count_snap = task_count;
    sched_task_t snap[SCHED_MAX_TASKS];
    uint64_t totals[SCHED_MAX_TASKS];
    memcpy(snap,   tasks,        sizeof(snap));
    memcpy(totals, task_total_us, sizeof(totals));
    hal_irq_restore(saved);

    uint64_t grand_total = core1_total_us;
    if (grand_total == 0) grand_total = 1;

    printf("ID  ENABLED  INTERVAL  CPU%%    NAME\n");
    for (int i = 0; i < count_snap; i++) {
        uint32_t pct_x10 = (uint32_t)((totals[i] * 1000) / grand_total);
        printf(" %d   %-5s   %4lu ms   %2lu.%lu%%  %s\n",
               i,
               snap[i].enabled ? "yes" : "no",
               (unsigned long)snap[i].interval_ms,
               (unsigned long)(pct_x10 / 10), (unsigned long)(pct_x10 % 10),
               snap[i].name);
    }
}

int sched_snapshot(sched_task_t* out, uint64_t* totals_out, int max) {
    uint32_t saved = hal_irq_disable();
    int n = task_count < max ? task_count : max;
    memcpy(out,        tasks,        n * sizeof(sched_task_t));
    memcpy(totals_out, task_total_us, n * sizeof(uint64_t));
    hal_irq_restore(saved);
    return n;
}

uint64_t sched_core1_total_us(void) { return core1_total_us; }
