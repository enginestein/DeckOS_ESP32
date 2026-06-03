#include <string.h>
#include "driver/spi_master.h"
#include "hal.h"

typedef struct {
    spi_host_device_t host;
    spi_device_handle_t dev;
    bool inited;
} spi_ctx_t;

static spi_ctx_t s_spi;

hal_spi_t hal_spi_init(uint sck, uint mosi, uint miso, uint baud) {
    if (s_spi.inited) return &s_spi;

    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    s_spi.host = SPI2_HOST;
    esp_err_t err = spi_bus_initialize(s_spi.host, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return NULL;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = baud,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    err = spi_bus_add_device(s_spi.host, &devcfg, &s_spi.dev);
    if (err != ESP_OK) { spi_bus_free(s_spi.host); return NULL; }

    s_spi.inited = true;
    return &s_spi;
}

void hal_spi_deinit(hal_spi_t bus) {
    if (!bus) return;
    spi_ctx_t* ctx = (spi_ctx_t*)bus;
    if (ctx->inited) {
        spi_bus_remove_device(ctx->dev);
        spi_bus_free(ctx->host);
        memset(ctx, 0, sizeof(*ctx));
    }
}

int hal_spi_transfer(hal_spi_t bus, uint cs_pin,
                     const uint8_t* tx, uint8_t* rx, size_t len) {
    if (!bus || !len) return -1;
    spi_ctx_t* ctx = (spi_ctx_t*)bus;
    if (!ctx->inited) return -1;

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    hal_gpio_put(cs_pin, 0);
    esp_err_t err = spi_device_transmit(ctx->dev, &t);
    hal_gpio_put(cs_pin, 1);

    return (err == ESP_OK) ? (int)len : -1;
}
