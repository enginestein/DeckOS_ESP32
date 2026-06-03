#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "hal.h"
#include "kernel.h"

static void kernel_task(void* param) {
    (void)param;

    // Wait for console
    hal_console_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    hal_board_detect();
    printf("\n=== DeckOS for ESP32 ===\n");
    printf("Board: %s\n", hal_board_name());
    printf("Flash: %lu KB\n", hal_board_flash_size() / 1024);
    printf("PSRAM: %lu KB\n", hal_board_psram_size() / 1024);

    kernel_init();
    kernel_run();

    vTaskDelete(NULL);
}

void app_main(void) {
    // Init NVS first (needed by WiFi, BT)
    hal_nvs_init();
    hal_spiffs_init();

    xTaskCreatePinnedToCore(
        kernel_task,
        "kernel",
        32768,
        NULL,
        5,
        NULL,
        0  // Core 0
    );
}
