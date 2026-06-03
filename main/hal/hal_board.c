#include <string.h>
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "soc/efuse_reg.h"
#include "soc/soc.h"
#include "hal.h"

// Chip package detection
static uint32_t get_chip_package(void) {
    return REG_READ(EFUSE_BLK0_RDATA3_REG) >> 9 & 0x0F;
}

board_id_t hal_board_detect(void) {
    esp_chip_info_t info;
    esp_chip_info(&info);
    if (info.model != CHIP_ESP32) return BOARD_ESP32_WROOM;

    uint32_t package = get_chip_package();
    bool has_psram = (hal_board_psram_size() > 0);
    (void)package;

    if (has_psram) {
        // Boards with PSRAM and camera connector are ESP32-CAM
        return BOARD_ESP32_CAM;
    }
    return BOARD_ESP32_WROOM;
}

const char* hal_board_name(void) {
    switch (hal_board_detect()) {
        case BOARD_ESP32_WROOM: return "ESP32-WROOM";
        case BOARD_ESP32_CAM:   return "ESP32-CAM (Robodo/AI-Thinker)";
        case BOARD_ESP32_S3:    return "ESP32-S3";
        default:                return "Unknown ESP32";
    }
}

uint32_t hal_board_flash_size(void) {
    uint32_t size = 0;
    esp_flash_get_size(NULL, &size);
    return size;
}

uint32_t hal_board_psram_size(void) {
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

bool hal_board_has_camera(void) {
    return (hal_board_detect() == BOARD_ESP32_CAM);
}

bool hal_board_has_sdcard(void) {
    return (hal_board_detect() == BOARD_ESP32_CAM);
}
