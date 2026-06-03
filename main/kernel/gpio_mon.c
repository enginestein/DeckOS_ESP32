#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "gpio_mon.h"
#include "syslog.h"

#define GPIO_MON_SLOTS 128

typedef struct {
    uint32_t timestamp_ms;
    uint8_t  pin;
    uint8_t  edge;
} gpio_event_t;

typedef struct {
    gpio_event_t events[GPIO_MON_SLOTS];
    int          head;
    int          count;
    bool         active;
} pin_mon_t;

static pin_mon_t s_monitors[29];

static void __attribute__((unused)) gpio_irq_cb(uint gpio, uint32_t events) {
    if (gpio > 28) return;
    pin_mon_t* m = &s_monitors[gpio];
    if (!m->active) return;

    gpio_event_t* e = &m->events[m->head];
    e->timestamp_ms = hal_time_ms();
    e->pin          = (uint8_t)gpio;
    e->edge         = (events & 0x4) ? 1 : 0;  // 0x4 = GPIO_IRQ_EDGE_RISE

    m->head = (m->head + 1) % GPIO_MON_SLOTS;
    if (m->count < GPIO_MON_SLOTS) m->count++;

    char buf[32];
    snprintf(buf, sizeof(buf), "GPIO%d %s edge",
             gpio, e->edge ? "RISING" : "FALLING");
    syslog_write(LOG_INFO, "gpio_irq", buf);
}

bool gpio_mon_start(uint pin, uint timeout_s) {
    (void)timeout_s;
    if (pin > 28) return false;
    pin_mon_t* m = &s_monitors[pin];
    memset(m, 0, sizeof(*m));
    m->active = true;

    hal_gpio_init(pin);
    hal_gpio_set_dir(pin, false);
    hal_gpio_set_pull(pin, true, false);

    char buf[24];
    snprintf(buf, sizeof(buf), "monitoring GPIO%d", pin);
    LOG_I("gpio_irq", buf);
    return true;
}

void gpio_mon_stop(uint pin) {
    if (pin > 28) return;
    s_monitors[pin].active = false;
    char buf[24];
    snprintf(buf, sizeof(buf), "stopped GPIO%d", pin);
    LOG_I("gpio_irq", buf);
}

void gpio_mon_dump(uint pin) {
    if (pin > 28) return;
    pin_mon_t* m = &s_monitors[pin];

    if (m->count == 0) {
        printf("GPIO%d: no events captured\n", pin);
        return;
    }

    printf("GPIO%d event log (%d events):\n", pin, m->count);
    printf("  TIME (ms)    EDGE\n");

    int start = (m->head - m->count + GPIO_MON_SLOTS * 2) % GPIO_MON_SLOTS;
    for (int i = 0; i < m->count; i++) {
        gpio_event_t* e = &m->events[(start + i) % GPIO_MON_SLOTS];
        printf("  %10lu   %s\n",
               (unsigned long)e->timestamp_ms,
               e->edge ? "RISING ^" : "FALLING v");
    }
    m->head  = 0;
    m->count = 0;
}

bool gpio_mon_active(uint pin) {
    return (pin <= 28) && s_monitors[pin].active;
}

void gpio_mon_watch(uint pin, uint32_t timeout_ms) {
    if (!gpio_mon_start(pin, 0)) {
        printf("failed to start monitor on GPIO%d\n", pin);
        return;
    }

    printf("watching GPIO%d  (press any key to stop)...\n", pin);
    uint32_t start_ms = hal_time_ms();
    int      last_val = hal_gpio_get(pin);
    uint32_t edge_count = 0;

    while (true) {
        int c = hal_console_getchar();
        if (c >= 0) break;

        pin_mon_t* m = &s_monitors[pin];
        if (m->count > 0) {
            int s2 = (m->head - m->count + GPIO_MON_SLOTS * 2) % GPIO_MON_SLOTS;
            for (int i = 0; i < m->count; i++) {
                gpio_event_t* e = &m->events[(s2 + i) % GPIO_MON_SLOTS];
                printf("  [%7lu ms] GPIO%d %s\n",
                       (unsigned long)e->timestamp_ms,
                       pin,
                       e->edge ? "RISING ^" : "FALLING v");
                edge_count++;
            }
            m->head  = 0;
            m->count = 0;
        }

        if (timeout_ms && (hal_time_ms() - start_ms) >= timeout_ms)
            break;

        hal_sleep_ms(1);
    }

    gpio_mon_stop(pin);
    printf("GPIO%d monitor stopped  (%lu edges captured)\n", pin, (unsigned long)edge_count);
}
