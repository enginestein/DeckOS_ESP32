#include <stdio.h>
#include "hal.h"
#include "uart_pass.h"
#include "kernel.h"

void uart_passthrough(uint tx_pin, uint rx_pin, uint baud, uint32_t timeout_ms) {
    hal_uart_init(tx_pin, rx_pin, baud, NULL);

    printf("[uart] passthrough TX=GP%d RX=GP%d %u baud\n",
           tx_pin, rx_pin, baud);
    printf("       Press Ctrl-X (0x18) to exit.\n");

    uint32_t start_ms = hal_time_ms();
    uint32_t rx_total = 0;
    uint32_t tx_total = 0;

    while (true) {
        int c = hal_console_getchar();
        if (c >= 0) {
            if (c == 0x18) {
                printf("\n[uart] passthrough ended (rx=%lu tx=%lu bytes)\n",
                       rx_total, tx_total);
                break;
            }
            hal_uart_write_char((char)c);
            tx_total++;
        }

        int ch = hal_uart_read_char();
        if (ch >= 0) {
            putchar(ch);
            rx_total++;
        }

        if (timeout_ms && (hal_time_ms() - start_ms) >= timeout_ms) {
            printf("\n[uart] passthrough timeout\n");
            break;
        }

        kernel_poll();
        hal_sleep_us(100);
    }

    hal_uart_deinit();
}
