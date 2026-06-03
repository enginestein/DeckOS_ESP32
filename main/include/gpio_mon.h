#pragma once
#include <stdint.h>
#include <stdbool.h>

bool gpio_mon_start(uint pin, uint timeout_s);
void gpio_mon_stop(uint pin);
void gpio_mon_dump(uint pin);
void gpio_mon_tick(void);
void gpio_mon_stop_all(void);
