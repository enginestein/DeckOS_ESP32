#include <stdio.h>
#include <unistd.h>
#include <sys/unistd.h>
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "xtensa/xtruntime.h"
#include "hal.h"

void hal_reboot(void) {
    esp_restart();
}

void hal_reboot_dfu(void) {
    printf("[hal] DFU boot not supported on ESP32, rebooting normally\n");
    esp_restart();
}

void hal_watchdog_enable(uint timeout_ms) {
    esp_task_wdt_config_t cfg = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = (1 << 0),
        .trigger_panic = true,
    };
    esp_task_wdt_init(&cfg);
    esp_task_wdt_add(NULL);
}

void hal_watchdog_update(void) {
    esp_task_wdt_reset();
}

hal_irq_state_t hal_irq_disable(void) {
    return XTOS_SET_INTLEVEL(3);
}

void hal_irq_restore(hal_irq_state_t state) {
    XTOS_RESTORE_INTLEVEL(state);
}

void hal_console_init(void) {
}

bool hal_console_connected(void) {
    return true;
}

int hal_console_getchar(void) {
    char c;
    int ret = read(STDIN_FILENO, &c, 1);
    if (ret == 1) return (int)(unsigned char)c;
    return -1;
}

void hal_console_putchar(char c) {
    write(STDOUT_FILENO, &c, 1);
}
