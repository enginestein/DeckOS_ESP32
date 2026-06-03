#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "hal.h"

#define SPI_DEFAULT_SCK   2
#define SPI_DEFAULT_MOSI  3
#define SPI_DEFAULT_MISO  4
#define SPI_DEFAULT_CS    5
#define SPI_DEFAULT_BAUD  1000000

hal_spi_t spi_bus_get(void);
void      spi_bus_init(uint sck, uint mosi, uint miso, uint baud);
void      spi_bus_deinit(void);
int       spi_bus_transfer(uint cs_pin, const uint8_t* tx, uint8_t* rx, size_t len);
void      spi_bus_write_reg(uint cs_pin, uint8_t reg, uint8_t val);
uint8_t   spi_bus_read_reg(uint cs_pin, uint8_t reg);
