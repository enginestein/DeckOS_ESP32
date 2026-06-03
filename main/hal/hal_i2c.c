#include <string.h>
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "hal.h"

#define I2C_MASTER_TX_BUF  0
#define I2C_MASTER_RX_BUF  0
#define I2C_MASTER_TIMEOUT 1000

static i2c_port_t i2c_port = I2C_NUM_0;
static bool i2c_inited = false;

bool hal_i2c_init(uint sda, uint scl, uint freq_hz) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = freq_hz,
    };
    esp_err_t err = i2c_param_config(i2c_port, &conf);
    if (err != ESP_OK) return false;
    err = i2c_driver_install(i2c_port, I2C_MODE_MASTER,
                             I2C_MASTER_RX_BUF, I2C_MASTER_TX_BUF, 0);
    if (err != ESP_OK) return false;
    i2c_inited = true;
    return true;
}

bool hal_i2c_scan(uint sda, uint scl, uint8_t* addrs, int max) {
    if (!i2c_inited) hal_i2c_init(sda, scl, 100000);
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, I2C_MASTER_TIMEOUT);
        i2c_cmd_link_delete(cmd);
        if (err == ESP_OK && count < max) {
            addrs[count++] = addr;
        }
    }
    return count > 0;
}

int hal_i2c_write(uint addr, const uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t*)data, len, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, I2C_MASTER_TIMEOUT);
    i2c_cmd_link_delete(cmd);
    return (err == ESP_OK) ? (int)len : -1;
}

int hal_i2c_read(uint addr, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, I2C_MASTER_TIMEOUT);
    i2c_cmd_link_delete(cmd);
    return (err == ESP_OK) ? (int)len : -1;
}

int hal_i2c_write_read(uint addr, const uint8_t* wdata, size_t wlen,
                        uint8_t* rdata, size_t rlen) {
    int ret = hal_i2c_write(addr, wdata, wlen);
    if (ret < 0) return ret;
    return hal_i2c_read(addr, rdata, rlen);
}
