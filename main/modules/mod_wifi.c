#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "wifi.h"

static void cmd_wifi(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  wifi init                    init WiFi subsystem\n");
        printf("  wifi ap <ssid> [pass]        start softAP mode\n");
        printf("  wifi scan                    scan for networks\n");
        printf("  wifi join <ssid> <pass>      connect to network\n");
        printf("  wifi disconnect              disconnect\n");
        printf("  wifi status                  show connection state\n");
        printf("  wifi get <url>               HTTP GET\n");
        printf("  wifi post <url> <body>       HTTP POST\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (wifi_init()) printf("WiFi initialised\n");
        else printf("WiFi init FAILED\n");
    } else if (strcmp(argv[1], "ap") == 0 && argc >= 3) {
        const char* pass = (argc >= 4) ? argv[3] : "";
        if (wifi_ap_start(argv[2], pass))
            printf("AP '%s' started at 192.168.4.1\n", argv[2]);
        else printf("AP start failed\n");
    } else if (strcmp(argv[1], "scan") == 0) {
        wifi_ap_t aps[20];
        int count = 0;
        if (wifi_scan(aps, 20, &count)) {
            printf("Networks found: %d\n", count);
            for (int i = 0; i < count; i++)
                printf("  %-32s  RSSI: %4d dBm\n", aps[i].ssid, aps[i].rssi);
        } else printf("scan failed\n");
    } else if (strcmp(argv[1], "join") == 0 && argc >= 4) {
        if (wifi_connect(argv[2], argv[3]))
            printf("Connected! IP: %s\n", wifi_get_ip());
        else printf("Connection failed\n");
    } else if (strcmp(argv[1], "disconnect") == 0) {
        wifi_disconnect();
        printf("Disconnected\n");
    } else if (strcmp(argv[1], "status") == 0) {
        wifi_print_status();
    } else if (strcmp(argv[1], "get") == 0 && argc >= 3) {
        char resp[2048];
        if (wifi_http_get(argv[2], resp, sizeof(resp)))
            printf("%s\n", resp);
        else printf("HTTP GET failed\n");
    } else if (strcmp(argv[1], "post") == 0 && argc >= 4) {
        char resp[2048];
        if (wifi_http_post(argv[2], argv[3], resp, sizeof(resp)))
            printf("%s\n", resp);
        else printf("HTTP POST failed\n");
    } else printf("unknown wifi: %s\n", argv[1]);
}

static module_cmd_t s_cmds[] = {
    {"wifi", "WiFi manager (init/ap/scan/join/disconnect/status/get/post)", cmd_wifi},
};

static bool mod_wifi_load(void) {
    if (!wifi_init()) {
        printf("wifi: init failed\n");
        return false;
    }
    printf("wifi: loaded\n");
    return true;
}

static void mod_wifi_unload(void) {
    wifi_disconnect();
    wifi_deinit();
    printf("wifi: unloaded\n");
}

plugin_api_t MOD_WIFI = {
    .init = mod_wifi_load,
    .deinit = mod_wifi_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
