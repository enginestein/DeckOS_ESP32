#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "hal.h"
#include "tone.h"

static uint8_t  s_active_pin  = 0xFF;
static uint32_t s_active_freq = 0;

static const uint32_t note_hz_oct4[12] = {
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494
};
static const char* note_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

uint32_t tone_note_to_hz(const char* note) {
    if (!note) return 0;
    if (strcasecmp(note, "REST") == 0 || toupper(note[0]) == 'R') return 0;

    char name[4] = {0};
    int  ni = 0;
    int  i  = 0;
    while (note[i] && !isdigit((unsigned char)note[i]) && ni < 3)
        name[ni++] = toupper((unsigned char)note[i++]);
    name[ni] = '\0';
    int octave = isdigit((unsigned char)note[i]) ? (note[i] - '0') : 4;

    if (ni >= 2 && name[1] == 'B') { name[1] = '#'; name[0]--; }

    int semitone = -1;
    for (int s = 0; s < 12; s++)
        if (strcasecmp(name, note_names[s]) == 0) { semitone = s; break; }
    if (semitone < 0) return 0;

    uint32_t hz = note_hz_oct4[semitone];
    int diff = octave - 4;
    if (diff > 0) hz <<= diff;
    else if (diff < 0) hz >>= (-diff);
    return hz;
}

void tone_play_hz(uint pin, uint hz, uint ms) {
    if (hz == 0) {
        hal_pwm_deinit((uint8_t)pin);
        hal_sleep_ms(ms);
        return;
    }

    hal_pwm_init((uint8_t)pin);
    hal_pwm_set_duty((uint8_t)pin, 50.0f, hz);
    s_active_pin  = (uint8_t)pin;
    s_active_freq = hz;

    hal_sleep_ms(ms);
    hal_pwm_set_duty((uint8_t)pin, 0, hz);
}

void tone_play(uint pin, const char* note, uint ms) {
    uint32_t hz = tone_note_to_hz(note);
    tone_play_hz(pin, hz, ms);
}

void tone_stop(void) {
    uint8_t pin = s_active_pin;
    if (pin == 0xFF) return;
    hal_pwm_deinit(pin);
    hal_gpio_init(pin);
    hal_gpio_set_dir(pin, true);
    hal_gpio_put(pin, false);
    s_active_pin  = 0xFF;
    s_active_freq = 0;
}

void tone_melody(uint8_t pin, const char* sequence) {
    if (!sequence) return;
    char buf[2048];
    strncpy(buf, sequence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* saveptr = NULL;
    char* tok = strtok_r(buf, " ", &saveptr);
    while (tok) {
        char* colon = strchr(tok, ':');
        uint32_t ms = 200;
        if (colon) {
            *colon = '\0';
            ms = (uint32_t)atoi(colon + 1);
            if (ms < 10) ms = 10;
        }
        uint32_t hz = tone_note_to_hz(tok);
        printf("  %s -> %lu Hz, %lu ms\n", tok, (unsigned long)hz, (unsigned long)ms);
        tone_play_hz(pin, hz, ms);
        if      (ms >= 300) hal_sleep_ms(12);
        else if (ms >= 150) hal_sleep_ms(8);
        else                hal_sleep_ms(4);
        tok = strtok_r(NULL, " ", &saveptr);
    }
    tone_stop();
}
