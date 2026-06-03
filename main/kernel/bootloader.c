#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "bootloader.h"
#include "config.h"

flash_config_t g_config;

void bootloader_run(void) {
    bool had_valid = config_load(&g_config);
    if (!had_valid)
        printf("[boot] flash config blank - using defaults\n");

    board_id_t board = hal_board_detect();
    printf("[boot] board: %s\n", hal_board_name());

    if (g_config.boot_led) {
        hal_gpio_init(33);
        hal_gpio_set_dir(33, true);
        hal_gpio_put(33, true);
    }

    bootloader_print_banner(&g_config);
}

void bootloader_print_banner(const flash_config_t* cfg) {
    printf("\n");
    printf("  +----------------------------------+\n");
    printf("  |                                  |\n");
    printf("  |         DeckOS v6.0              |\n");
    printf("  |         ESP32 Port               |\n");
    printf("  |                                  |\n");
    printf("  +----------------------------------+\n");
    printf("  host   : %s\n", cfg->hostname);
    printf("  board  : %s\n", hal_board_name());
    printf("\n");
}

bool bootloader_check_recovery(void) {
    // Recovery mode can be triggered via GPIO button or NVS flag
    uint32_t recovery = 0;
    if (hal_nvs_get_u32("recovery", &recovery) && recovery) {
        hal_nvs_set_u32("recovery", 0);
        hal_nvs_commit();
        printf("[boot] RECOVERY MODE\n\n");
        return true;
    }
    return false;
}
