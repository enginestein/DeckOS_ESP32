#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef unsigned int uint;

// Board identifiers
typedef enum {
    BOARD_UNKNOWN = 0,
    BOARD_RP2040_PICO,
    BOARD_ESP32_WROOM,
    BOARD_ESP32_CAM,
    BOARD_ESP32_S3,
} board_id_t;

board_id_t hal_board_detect(void);
const char* hal_board_name(void);
uint32_t   hal_board_flash_size(void);
uint32_t   hal_board_psram_size(void);
bool       hal_board_has_camera(void);
bool       hal_board_has_sdcard(void);

// Time
uint64_t hal_time_us(void);
uint32_t hal_time_ms(void);
void     hal_sleep_us(uint32_t us);
void     hal_sleep_ms(uint32_t ms);

// GPIO
void     hal_gpio_init(uint pin);
void     hal_gpio_set_dir(uint pin, bool out);
void     hal_gpio_put(uint pin, bool val);
bool     hal_gpio_get(uint pin);
void     hal_gpio_set_pull(uint pin, bool up, bool down);
void     hal_gpio_set_function(uint pin, uint8_t fn);
uint32_t hal_gpio_get_all(void);

// ADC
void     hal_adc_init(void);
void     hal_adc_select_input(uint channel);
uint16_t hal_adc_read(void);
float    hal_adc_read_voltage(uint channel);

// I2C
bool hal_i2c_init(uint sda, uint scl, uint freq_hz);
bool hal_i2c_scan(uint sda, uint scl, uint8_t* addrs, int max);
int  hal_i2c_write(uint addr, const uint8_t* data, size_t len);
int  hal_i2c_read(uint addr, uint8_t* data, size_t len);
int  hal_i2c_write_read(uint addr, const uint8_t* wdata, size_t wlen, uint8_t* rdata, size_t rlen);

// SPI
typedef void* hal_spi_t;
hal_spi_t hal_spi_init(uint sck, uint mosi, uint miso, uint baud);
void      hal_spi_deinit(hal_spi_t bus);
int       hal_spi_transfer(hal_spi_t bus, uint cs_pin, const uint8_t* tx, uint8_t* rx, size_t len);

// PWM / Servo
void hal_pwm_init(uint pin);
void hal_pwm_set_duty(uint pin, float duty_pct, uint freq_hz);
void hal_pwm_deinit(uint pin);

// UART
typedef void (*hal_uart_rx_cb_t)(char c);
bool hal_uart_init(uint tx_pin, uint rx_pin, uint baud, hal_uart_rx_cb_t cb);
void hal_uart_write_char(char c);
void hal_uart_write_str(const char* s);
int  hal_uart_read_char(void);
void hal_uart_deinit(void);

// Flash / NVS
bool hal_nvs_init(void);
bool hal_nvs_get_str(const char* key, char* buf, size_t bufsize);
bool hal_nvs_set_str(const char* key, const char* val);
bool hal_nvs_get_u32(const char* key, uint32_t* val);
bool hal_nvs_set_u32(const char* key, uint32_t val);
bool hal_nvs_get_blob(const char* key, uint8_t* buf, size_t* len);
bool hal_nvs_set_blob(const char* key, const uint8_t* buf, size_t len);
bool hal_nvs_commit(void);
void hal_nvs_erase_all(void);

// SPIFFS (VFS persistence)
bool hal_spiffs_init(void);
bool hal_spiffs_read(const char* path, uint8_t* buf, size_t* len);
bool hal_spiffs_write(const char* path, const uint8_t* buf, size_t len);
bool hal_spiffs_delete(const char* path);
bool hal_spiffs_stat(const char* path, size_t* out_size);
void hal_spiffs_list(const char* dir);

// LED (onboard / flash)
void hal_led_init(uint pin);
void hal_led_set(bool on);

// Camera (ESP32-CAM)
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t* buf;
    size_t   len;
    bool     jpeg;
} hal_camera_frame_t;

bool hal_camera_init(uint pin_xclk, uint pin_sioc, uint pin_siod,
                     uint pin_pwdn, uint pin_reset, uint pin_vsync,
                     uint pin_href, uint pin_pclk, uint pin_d0,
                     uint pins_d1_d7, ...);
bool hal_camera_capture(hal_camera_frame_t* frame);
void hal_camera_return_frame(hal_camera_frame_t* frame);
void hal_camera_set_resolution(uint16_t w, uint16_t h);
bool hal_camera_is_ready(void);

// Reset / Reboot
void hal_reboot(void);
void hal_reboot_dfu(void);
void hal_watchdog_enable(uint timeout_ms);
void hal_watchdog_update(void);

// Console / stdio
void hal_console_init(void);
bool hal_console_connected(void);
int  hal_console_getchar(void);
void hal_console_putchar(char c);

// Critical sections
typedef uint32_t hal_irq_state_t;
hal_irq_state_t hal_irq_disable(void);
void            hal_irq_restore(hal_irq_state_t state);
