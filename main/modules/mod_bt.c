#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "bt.h"

static void cmd_bt(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  bt init              init Bluetooth (SPP)\n");
        printf("  bt deinit            power down Bluetooth\n");
        printf("  bt status            show connection state\n");
        printf("  bt shell             interactive BT shell\n");
        printf("  bt log               mirror logs to BT\n");
        printf("  bt exec <cmd>        execute command remotely\n");
        printf("  bt top <ms>          stream top over BT\n");
        printf("  bt send <path>       send file over BT\n");
        printf("  bt recv <path>       receive file over BT\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (bt_init(115200)) printf("Bluetooth initialised\n");
        else printf("BT init FAILED\n");
    } else if (strcmp(argv[1], "deinit") == 0) {
        bt_deinit();
        printf("Bluetooth deinitialised\n");
    } else if (strcmp(argv[1], "status") == 0) {
        printf("BT: %s\n", bt_is_connected() ? "connected" : "disconnected");
    } else if (strcmp(argv[1], "shell") == 0) {
        bt_shell();
    } else if (strcmp(argv[1], "log") == 0) {
        printf("BT log mirror: %s\n", bt_log_is_enabled() ? "enabled" : "disabled");
    } else if (strcmp(argv[1], "exec") == 0 && argc >= 3) {
        bt_exec(argv[2]);
    } else if (strcmp(argv[1], "top") == 0 && argc >= 3) {
        bt_top_start(atoi(argv[2]));
    } else if (strcmp(argv[1], "send") == 0 && argc >= 3) {
        bt_send_file(argv[2]);
    } else if (strcmp(argv[1], "recv") == 0 && argc >= 3) {
        bt_recv_file(argv[2]);
    } else printf("unknown bt: %s\n", argv[1]);
}

static module_cmd_t s_cmds[] = {
    {"bt", "Bluetooth SPP (init/deinit/status/shell/log/exec/top/send/recv)", cmd_bt},
};

static bool mod_bt_load(void) {
    if (!bt_init(115200)) {
        printf("bt: init failed\n");
        return false;
    }
    printf("bt: loaded\n");
    return true;
}

static void mod_bt_unload(void) {
    bt_deinit();
    printf("bt: unloaded\n");
}

plugin_api_t MOD_BT = {
    .init = mod_bt_load,
    .deinit = mod_bt_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
