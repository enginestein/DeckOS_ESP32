#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "module.h"
#include "commands.h"
#include "hal.h"
#include "tone.h"

static void cmd_tone(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: tone <pin> <note|Hz> [duration_ms]\n");
        printf("Notes: C4 D4 E4 F4 G4 A4 B4 C5 ... (or Hz like 440)\n");
        return;
    }
    int pin = atoi(argv[1]);
    const char *note_or_hz = argv[2];
    int ms = (argc >= 4) ? atoi(argv[3]) : 500;
    if (ms > 30000) ms = 30000;
    if (isdigit((unsigned char)note_or_hz[0])) {
        int hz = atoi(note_or_hz);
        if (hz < 20 || hz > 20000) { printf("Hz out of range (20-20000)\n"); return; }
        tone_play_hz(pin, (uint)hz, (uint)ms);
        printf("Tone %d Hz on GP%d for %d ms\n", hz, pin, ms);
    } else {
        tone_play(pin, note_or_hz, (uint)ms);
        printf("Note %s on GP%d for %d ms\n", note_or_hz, pin, ms);
    }
}

static void cmd_melody(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: melody <pin> <C4:200 E4:200 ...> | elise | canon | twinkle\n");
        return;
    }
    int pin = atoi(argv[1]);
    if (argc == 3 && (strcmp(argv[2], "elise") == 0 ||
                      strcmp(argv[2], "canon") == 0 ||
                      strcmp(argv[2], "twinkle") == 0)) {
        const char *notes[][2] = {
            {"elise", "E5:150 Eb5:150 E5:150 Eb5:150 E5:150 B4:150 D5:150 C5:150 A4:600"},
            {"canon", "C4:300 G3:300 A3:300 E3:300 F3:300 C3:300 F3:300 G3:300"},
            {"twinkle", "C4:200 C4:200 G4:200 G4:200 A4:200 A4:200 G4:400 F4:200 F4:200 E4:200 E4:200 D4:200 D4:200 C4:400"},
        };
        for (int i = 0; i < 3; i++) {
            if (strcmp(argv[2], notes[i][0]) == 0) {
                char buf[128];
                strncpy(buf, notes[i][1], sizeof(buf) - 1);
                char *tok = strtok(buf, " ");
                while (tok) {
                    char *colon = strchr(tok, ':');
                    if (colon) {
                        *colon++ = '\0';
                        tone_play(pin, tok, (uint)atoi(colon));
                        hal_sleep_ms((uint)atoi(colon) + 30);
                    }
                    tok = strtok(NULL, " ");
                }
                return;
            }
        }
    } else {
        for (int a = 2; a < argc; a++) {
            char *colon = strchr(argv[a], ':');
            if (colon) {
                *colon = '\0';
                int ms = atoi(colon + 1);
                tone_play(pin, argv[a], (uint)ms);
                hal_sleep_ms((uint)ms + 30);
                *colon = ':';
            }
        }
    }
}

static void cmd_piano(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: piano <pin>\n"); return; }
    int pin = atoi(argv[1]);
    printf("Piano mode on GP%d (press Q to quit)\n", pin);
    static const char *notes[] = {"C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4"};
    static const char keymap[] = {'a','w','s','e','d','f','t','g','y','h','u','j',0};
    while (true) {
        int c = getchar();
        if (c < 0) break;
        if (c == 'q' || c == 'Q') break;
        for (int i = 0; keymap[i]; i++) {
            if (c == (int)keymap[i]) {
                tone_play(pin, notes[i], 500);
                break;
            }
        }
    }
    tone_stop();
}

static module_cmd_t s_cmds[] = {
    {"tone", "Play tones (tone <pin> <Hz|note> [ms])", cmd_tone},
    {"melody", "Play melodies (elise/canon/twinkle or sequence)", cmd_melody},
    {"piano", "Interactive piano keyboard", cmd_piano},
};

static bool mod_tone_load(void) {
    printf("tone: loaded\n");
    return true;
}

static void mod_tone_unload(void) {
    tone_stop();
    printf("tone: unloaded\n");
}

plugin_api_t MOD_TONE = {
    .init = mod_tone_load,
    .deinit = mod_tone_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
