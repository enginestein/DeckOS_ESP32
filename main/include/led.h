#pragma once
#include <stdbool.h>

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);
void led_set(bool on);
void led_blink(int times, int ms);
