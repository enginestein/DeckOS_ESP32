#pragma once
#include <stdint.h>

void uart_passthrough(uint tx_pin, uint rx_pin, uint baud, uint32_t timeout_ms);
