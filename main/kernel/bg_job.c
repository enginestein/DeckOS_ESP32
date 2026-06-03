#include <stdio.h>
#include <string.h>
#include "bg_job.h"
#include "hal.h"
#include "print_lock.h"

#define BG_JOB_MAX MAX_BG_JOBS
#define BG_JOB_NAME_LEN 24

typedef enum {
    BG_JOB_IDLE = 0,
    BG_JOB_RUNNING,
    BG_JOB_DONE,
    BG_JOB_ERROR,
} bg_job_state_t;

typedef struct {
    char          name[BG_JOB_NAME_LEN];
    bg_job_state_t state;
    void        (*fn)(void*);
    void         *arg;
    bool          cancel;
} bg_job_entry_t;

static bg_job_entry_t s_jobs[BG_JOB_MAX];

void bg_job_init(void) {
    memset(s_jobs, 0, sizeof(s_jobs));
}

void bg_job_tick(void) {
    for (int i = 0; i < BG_JOB_MAX; i++) {
        uint32_t saved = hal_irq_disable();
        bg_job_entry_t snap = s_jobs[i];
        hal_irq_restore(saved);

        if (snap.state != BG_JOB_RUNNING) continue;
        snap.fn(snap.arg);
        uint32_t s2 = hal_irq_disable();
        if (s_jobs[i].state == BG_JOB_RUNNING)
            s_jobs[i].state = BG_JOB_DONE;
        hal_irq_restore(s2);

        print_lock_acquire();
        printf("\n[bg] job '%s' finished\n", snap.name);
        print_lock_release();
    }
}

int bg_job_start(void (*fn)(void*), void* arg, uint32_t timeout_ms) {
    (void)timeout_ms;
    uint32_t saved = hal_irq_disable();
    for (int i = 0; i < BG_JOB_MAX; i++) {
        if (s_jobs[i].state == BG_JOB_IDLE ||
            s_jobs[i].state == BG_JOB_DONE ||
            s_jobs[i].state == BG_JOB_ERROR) {
            snprintf(s_jobs[i].name, BG_JOB_NAME_LEN, "job%d", i);
            s_jobs[i].fn     = fn;
            s_jobs[i].arg    = arg;
            s_jobs[i].cancel = false;
            s_jobs[i].state  = BG_JOB_RUNNING;
            hal_irq_restore(saved);
            printf("[bg] job started in slot %d\n", i);
            return i;
        }
    }
    hal_irq_restore(saved);
    printf("[bg] job queue full (%d slots)\n", BG_JOB_MAX);
    return -1;
}

void bg_job_list(void) {
    static const char* state_names[] = {"idle", "running", "done", "cancelled"};
    printf("ID  STATE    NAME\n");
    printf("--  -------  --------------------\n");
    for (int i = 0; i < BG_JOB_MAX; i++) {
        uint32_t saved = hal_irq_disable();
        bg_job_entry_t snap = s_jobs[i];
        hal_irq_restore(saved);
        printf(" %d  %-7s  %s\n", i,
               state_names[snap.state],
               snap.state != BG_JOB_IDLE ? snap.name : "-");
    }
}

bool bg_job_cancel(int id) {
    if (id < 0 || id >= BG_JOB_MAX) { printf("invalid job id\n"); return false; }
    uint32_t saved = hal_irq_disable();
    s_jobs[id].cancel = true;
    s_jobs[id].state  = BG_JOB_IDLE;
    hal_irq_restore(saved);
    printf("[bg] job %d cancelled\n", id);
    return true;
}
