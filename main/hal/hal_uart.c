#include <string.h>
#include "driver/uart.h"
#include "hal.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE 256

static QueueHandle_t uart_queue;
static hal_uart_rx_cb_t rx_callback = NULL;

static void uart_event_task(void* pvParameters) {
    uart_event_t event;
    while (1) {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                uint8_t buf[BUF_SIZE];
                int len = uart_read_bytes(UART_NUM, buf, event.size, 0);
                if (rx_callback) {
                    for (int i = 0; i < len; i++)
                        rx_callback((char)buf[i]);
                }
            }
        }
    }
}

bool hal_uart_init(uint tx_pin, uint rx_pin, uint baud, hal_uart_rx_cb_t cb) {
    uart_config_t conf = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t err = uart_param_config(UART_NUM, &conf);
    if (err != ESP_OK) return false;

    err = uart_set_pin(UART_NUM, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return false;

    err = uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE, 10, &uart_queue, 0);
    if (err != ESP_OK) return false;

    rx_callback = cb;
    xTaskCreate(uart_event_task, "uart_evt", 2048, NULL, 10, NULL);
    return true;
}

void hal_uart_write_char(char c) {
    uart_write_bytes(UART_NUM, &c, 1);
}

void hal_uart_write_str(const char* s) {
    uart_write_bytes(UART_NUM, s, strlen(s));
}

int hal_uart_read_char(void) {
    uint8_t c;
    int len = uart_read_bytes(UART_NUM, &c, 1, 0);
    return len > 0 ? c : -1;
}

void hal_uart_deinit(void) {
    uart_driver_delete(UART_NUM);
    rx_callback = NULL;
}
