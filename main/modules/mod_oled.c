#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "oled.h"

static void cmd_oled(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  oled init [sda] [scl]    init SSD1306 I2C\n");
        printf("  oled on                  display on\n");
        printf("  oled off                 display off (power save)\n");
        printf("  oled clear               clear framebuffer\n");
        printf("  oled text <col> <row> <text>\n");
        printf("  oled line <x0> <y0> <x1> <y1>\n");
        printf("  oled rect <x> <y> <w> <h> [fill]\n");
        printf("  oled pixel <x> <y> [on]\n");
        printf("  oled inv [0|1]           invert display\n");
        printf("  oled scroll <speed>\n");
        printf("  oled char <c>            draw character\n");
        printf("  oled image <file.bin>\n");
        printf("  oled logo                DeckOS logo\n");
        printf("  oled demo                demo animation\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        uint sda = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint scl = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        if (oled_init(sda, scl)) printf("OLED initialised\n");
        else printf("OLED init FAILED\n");
    } else if (strcmp(argv[1], "on") == 0) {
        oled_on();
    } else if (strcmp(argv[1], "off") == 0) {
        oled_off();
    } else if (strcmp(argv[1], "clear") == 0) {
        oled_clear(); oled_flush();
    } else if (strcmp(argv[1], "text") == 0 && argc >= 5) {
        int col = atoi(argv[2]), row = atoi(argv[3]);
        oled_text(col, row, argv[4]);
        oled_flush();
    } else if (strcmp(argv[1], "pixel") == 0 && argc >= 4) {
        int on = (argc >= 5) ? atoi(argv[4]) : 1;
        oled_pixel(atoi(argv[2]), atoi(argv[3]), on);
        oled_flush();
    } else if (strcmp(argv[1], "inv") == 0) {
        oled_invert((argc >= 3) ? atoi(argv[2]) : 1);
    } else if (strcmp(argv[1], "line") == 0) {
        printf("oled line: not implemented\n");
    } else if (strcmp(argv[1], "rect") == 0) {
        printf("oled rect: not implemented\n");
    } else if (strcmp(argv[1], "scroll") == 0) {
        printf("oled scroll: not implemented\n");
    } else if (strcmp(argv[1], "char") == 0) {
        printf("oled char: not implemented\n");
    } else if (strcmp(argv[1], "image") == 0) {
        printf("oled image: not implemented\n");
    } else if (strcmp(argv[1], "logo") == 0) {
        printf("oled logo: not implemented\n");
    } else if (strcmp(argv[1], "demo") == 0) {
        printf("oled demo: not implemented\n");
    } else printf("unknown oled: %s\n", argv[1]);
}

static module_cmd_t s_cmds[] = {
    {"oled", "OLED SSD1306 display (init/on/off/clear/text/line/rect/pixel/inv/scroll/char/image/logo/demo)", cmd_oled},
};

static bool mod_oled_load(void) {
    printf("oled: loaded\n");
    return true;
}

static void mod_oled_unload(void) {
    oled_off();
    printf("oled: unloaded\n");
}

plugin_api_t MOD_OLED = {
    .init = mod_oled_load,
    .deinit = mod_oled_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
