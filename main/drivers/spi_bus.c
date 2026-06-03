#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "spi_bus.h"

static hal_spi_t s_bus = 0;
static bool s_inited = false;
static uint s_sck, s_mosi, s_miso;

hal_spi_t spi_bus_get(void) { return s_bus; }

void spi_bus_init(uint sck, uint mosi, uint miso, uint baud) {
    if (s_inited) return;
    s_bus = hal_spi_init(sck, mosi, miso, baud);
    s_sck = sck;
    s_mosi = mosi;
    s_miso = miso;
    printf("[spi] init  SCK=GP%d MOSI=GP%d MISO=GP%d @%lu Hz\n",
           sck, mosi, miso, (uint32_t)baud);
    s_inited = true;
}

void spi_bus_deinit(void) {
    if (!s_inited) return;
    hal_spi_deinit(s_bus);
    s_inited = false;
}

int spi_bus_transfer(uint cs_pin, const uint8_t* tx, uint8_t* rx, size_t len) {
    if (!len) return 0;

    bool manage_cs = (cs_pin != 0xFF);
    if (manage_cs) {
        hal_gpio_set_dir(cs_pin, true);
        hal_gpio_put(cs_pin, 0);
    }

    int rc = hal_spi_transfer(s_bus, cs_pin, tx, rx, len);

    if (manage_cs)
        hal_gpio_put(cs_pin, 1);

    return rc;
}

void spi_bus_write_reg(uint cs_pin, uint8_t reg, uint8_t val) {
    uint8_t tx[2] = { reg & 0x7F, val };
    spi_bus_transfer(cs_pin, tx, NULL, 2);
}

uint8_t spi_bus_read_reg(uint cs_pin, uint8_t reg) {
    uint8_t tx[2] = { reg | 0x80, 0x00 };
    uint8_t rx[2] = {0};
    spi_bus_transfer(cs_pin, tx, rx, 2);
    return rx[1];
}
