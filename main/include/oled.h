#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef unsigned int uint;

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

bool oled_init(uint sda, uint scl);
void oled_on(void);
void oled_off(void);
void oled_clear(void);
void oled_flush(void);
void oled_pixel(int x, int y, int on);
void oled_text(int col, int row, const char* s);
void oled_set_contrast(uint8_t val);
void oled_invert(int on);
void oled_display(void);
