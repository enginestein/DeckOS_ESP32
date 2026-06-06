#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "board_detect.h"
#include "morse.h"

static void cmd_morse(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: morse <text> [wpm]\n");
        return;
    }
    int wpm = (argc >= 3) ? atoi(argv[2]) : 8;
    if (wpm < 3 || wpm > 40) { printf("wpm: 3-40\n"); return; }
    const board_info_t *b = board_detect();
    morse_send(argv[1], wpm, b->led_pin);
    printf("Morse: %s (%d wpm)\n", argv[1], wpm);
}

static module_cmd_t s_cmds[] = {
    {"morse", "Send morse code via LED (morse <text> [wpm])", cmd_morse},
};

static bool mod_morse_load(void) {
    printf("morse: loaded\n");
    return true;
}

static void mod_morse_unload(void) {
    printf("morse: unloaded\n");
}

plugin_api_t MOD_MORSE = {
    .init = mod_morse_load,
    .deinit = mod_morse_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
