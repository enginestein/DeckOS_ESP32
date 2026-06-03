#pragma once
#include <stdint.h>
#include <stdbool.h>

void servo_init(void);
int  servo_set(uint pin, int angle);
void servo_write_angle(uint8_t pin, int angle);
void servo_release(uint8_t pin);
void servo_sweep_blocking(uint8_t pin, int from, int to, int step_ms);

int  servo_bg_add(uint8_t pin, const char* name);
int  servo_bg_find(uint8_t pin);
void servo_bg_set_sweep(int slot, int min_deg, int max_deg, int step_deg, uint32_t step_ms);
void servo_bg_set_goto(int slot, int target, uint32_t step_ms);
void servo_bg_stop(int slot);
void servo_bg_tick(void);
void servo_bg_list(void);
