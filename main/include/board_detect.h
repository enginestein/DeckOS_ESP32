#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char* name;
    bool        has_camera;
    bool        has_sdcard;
    bool        has_psram;
    bool        has_led;
    uint32_t    flash_kb;
    uint8_t     led_pin;
    uint8_t     default_i2c_sda;
    uint8_t     default_i2c_scl;
    uint8_t     default_spi_sck;
    uint8_t     default_spi_mosi;
    uint8_t     default_spi_miso;
    uint8_t     default_uart_tx;
    uint8_t     default_uart_rx;
} board_info_t;

const board_info_t* board_detect(void);
void                board_print(void);
