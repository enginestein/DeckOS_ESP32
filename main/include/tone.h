#pragma once

void tone_init(void);
void tone_play(uint pin, const char* note, uint ms);
void tone_play_hz(uint pin, uint hz, uint ms);
void tone_stop(void);
