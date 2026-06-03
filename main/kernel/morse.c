#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "hal.h"
#include "morse.h"

static const char* morse_alpha[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....",
    "..",   ".---", "-.-",  ".-..", "--",   "-.",   "---",  ".--.",
    "--.-", ".-.",  "...",  "-",    "..-",  "...-", ".--",  "-..-",
    "-.--", "--.."
};

static const char* morse_digit[10] = {
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----."
};

static const char morse_punc_chars[] = ".,?!/()-=+";
static const char* morse_punc_code[] = {
    ".-.-.-", "--..--", "..--..", "-.-.--", "-..-.",
    "-.--.",  "-.--.-", "-...-",  ".-.-.",  "+-."
};

static const char* char_to_morse(char c) {
    c = toupper((unsigned char)c);
    if (c >= 'A' && c <= 'Z') return morse_alpha[c - 'A'];
    if (c >= '0' && c <= '9') return morse_digit[c - '0'];
    for (int i = 0; morse_punc_chars[i]; i++)
        if (c == morse_punc_chars[i]) return morse_punc_code[i];
    return NULL;
}

void morse_send(const char* text, int wpm, uint pin) {
    if (!text) return;
    if (wpm < 1)  wpm = 1;
    if (wpm > 40) wpm = 40;

    uint32_t dot_ms = 1200 / wpm;

    hal_gpio_init(pin);
    hal_gpio_set_dir(pin, true);
    hal_gpio_put(pin, false);

    printf("morse [%d WPM, dot=%lu ms]: ", wpm, (unsigned long)dot_ms);

    for (int i = 0; text[i]; i++) {
        char c = text[i];

        if (c == ' ') {
            printf("  ");
            hal_sleep_ms(dot_ms * 7);
            continue;
        }

        const char* code = char_to_morse(c);
        if (!code) continue;

        printf("%c(%s) ", c, code);
        fflush(stdout);

        for (int j = 0; code[j]; j++) {
            hal_gpio_put(pin, true);
            hal_sleep_ms(code[j] == '-' ? dot_ms * 3 : dot_ms);
            hal_gpio_put(pin, false);
            if (code[j + 1]) hal_sleep_ms(dot_ms);
        }
        if (text[i + 1] && text[i + 1] != ' ')
            hal_sleep_ms(dot_ms * 3);
    }

    printf("\ndone.\n");
    hal_gpio_put(pin, false);
}
