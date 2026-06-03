#include "board_detect.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>

static const board_info_t BOARD_TABLE[] = {
    { "ESP32-WROOM",  false, false, false, true,  4096, 2, 33, 21, 22, 23, 19, 18, 5 },
    { "ESP32-CAM",    true,  true,  false, true,  4096, 0, 33, 14, 15, 14, 15, 12, 13 },
    { "ESP32-S3",     false, false, true,  true, 16384, 2, 48, 41, 42, 40, 38, 39, 43 },
};
static const int BOARD_COUNT = sizeof(BOARD_TABLE) / sizeof(BOARD_TABLE[0]);

static board_info_t s_board;

const board_info_t* board_detect(void) {
    (void)BOARD_COUNT;
    board_id_t id = hal_board_detect();
    uint32_t flash_kb = hal_board_flash_size();

    if (id >= BOARD_ESP32_WROOM && id <= BOARD_ESP32_S3) {
        int idx = id - BOARD_ESP32_WROOM;
        s_board = BOARD_TABLE[idx];
    } else {
        s_board = BOARD_TABLE[0];
    }
    s_board.flash_kb = flash_kb;
    s_board.name = hal_board_name();
    return &s_board;
}

void board_print(void) {
    const board_info_t* b = board_detect();
    printf("Board      : %s\n", b->name);
    printf("Flash      : %lu KB\n", b->flash_kb);
    printf("Camera     : %s\n", b->has_camera ? "yes" : "no");
    printf("SD Card    : %s\n", b->has_sdcard ? "yes" : "no");
    printf("PSRAM      : %s\n", b->has_psram ? "yes" : "no");
    printf("LED GPIO   : %d\n", b->led_pin);
}
