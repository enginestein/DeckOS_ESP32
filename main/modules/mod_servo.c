#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "servo.h"

static void cmd_servo(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  servo <pin> <angle_deg>         -- set servo angle\n");
        printf("  servo sweep <pin> [from] [to] [ms] -- sweep 0-180\n");
        printf("  servo bg add <pin> [name]        -- register bg servo\n");
        printf("  servo bg sweep <slot> [min] [max] [step] [ms]\n");
        printf("  servo bg goto <slot> <angle> [ms]\n");
        printf("  servo bg stop <slot>\n");
        printf("  servo bg list\n");
        return;
    }
    if (strcmp(argv[1], "sweep") == 0) {
        int pin = (argc >= 3) ? atoi(argv[2]) : -1;
        if (pin < 0) { printf("usage: servo sweep <pin> [from] [to] [step_ms]\n"); return; }
        int from = (argc >= 4) ? atoi(argv[3]) : 0;
        int to   = (argc >= 5) ? atoi(argv[4]) : 180;
        int ms   = (argc >= 6) ? atoi(argv[5]) : 20;
        servo_sweep_blocking((uint8_t)pin, from, to, ms);
    } else if (argc >= 2 && strcmp(argv[1], "bg") == 0) {
        if (argc < 3) { printf("usage: servo bg <add|sweep|goto|stop|list>\n"); return; }
        if (strcmp(argv[2], "add") == 0) {
            int pin = (argc >= 4) ? atoi(argv[3]) : -1;
            if (pin < 0) { printf("need pin\n"); return; }
            int slot = servo_bg_add((uint8_t)pin, (argc >= 5) ? argv[4] : NULL);
            if (slot >= 0) printf("servo bg added: slot %d, GP%d\n", slot, pin);
        } else if (strcmp(argv[2], "sweep") == 0) {
            if (argc < 4) { printf("usage: servo bg sweep <slot> [min] [max] [step_deg] [step_ms]\n"); return; }
            int slot = atoi(argv[3]);
            int minv = (argc >= 5) ? atoi(argv[4]) : 0;
            int maxv = (argc >= 6) ? atoi(argv[5]) : 180;
            int step = (argc >= 7) ? atoi(argv[6]) : 1;
            int ms   = (argc >= 8) ? atoi(argv[7]) : 20;
            servo_bg_set_sweep(slot, minv, maxv, step, (uint32_t)ms);
            printf("servo slot %d sweep set %d-%d step %d every %d ms\n", slot, minv, maxv, step, ms);
        } else if (strcmp(argv[2], "goto") == 0) {
            if (argc < 5) { printf("usage: servo bg goto <slot> <angle> [step_ms]\n"); return; }
            int slot = atoi(argv[3]), angle = atoi(argv[4]);
            int ms   = (argc >= 6) ? atoi(argv[5]) : 20;
            servo_bg_set_goto(slot, angle, (uint32_t)ms);
            printf("servo slot %d goto %d deg\n", slot, angle);
        } else if (strcmp(argv[2], "stop") == 0) {
            if (argc < 4) { printf("usage: servo bg stop <slot>\n"); return; }
            servo_bg_stop(atoi(argv[3]));
            printf("servo slot %d stopped\n", atoi(argv[3]));
        } else if (strcmp(argv[2], "list") == 0) {
            servo_bg_list();
        } else printf("unknown bg subcommand: %s\n", argv[2]);
    } else {
        int pin = atoi(argv[1]);
        int angle = (argc >= 3) ? atoi(argv[2]) : 90;
        int ret = servo_set(pin, angle);
        if (ret >= 0) printf("Servo GP%d set to %d deg\n", pin, angle);
        else printf("Servo error on GP%d\n", pin);
    }
}

static module_cmd_t s_cmds[] = {
    {"servo", "Servo control (pin/angle, sweep, bg add/sweep/goto/stop/list)", cmd_servo},
};

static bool mod_servo_load(void) {
    printf("servo: loaded\n");
    return true;
}

static void mod_servo_unload(void) {
    for (int i = 0; i < 8; i++) servo_bg_stop(i);
    printf("servo: unloaded\n");
}

plugin_api_t MOD_SERVO = {
    .init = mod_servo_load,
    .deinit = mod_servo_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
