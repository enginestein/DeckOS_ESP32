#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "hal.h"

uint64_t hal_time_us(void) {
    return esp_timer_get_time();
}

uint32_t hal_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void hal_sleep_us(uint32_t us) {
    if (us >= portTICK_PERIOD_MS * 1000) {
        vTaskDelay((us + 999) / 1000 / portTICK_PERIOD_MS);
    } else if (us > 0) {
        esp_rom_delay_us(us);
        esp_task_wdt_reset();
    }
}

void hal_sleep_ms(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}
