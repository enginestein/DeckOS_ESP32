#pragma once
#include <stdint.h>
#include <stdbool.h>

void uart_detect_run(uint8_t rx_pin, uint32_t timeout_ms);
void la_detect_protocol(uint8_t pin, int samples, int us_per_sample);
