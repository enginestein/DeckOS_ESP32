#pragma once
#include <stdbool.h>

void oled_console_enable(bool on);
bool oled_console_enabled(void);
void oled_console_write(const char* s);
void oled_console_poll(void);
