#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "nrf24l01.h"

static nrf24_t s_nrf;

static void cmd_nrf24(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  nrf24 init [csn] [ce]          init NRF24L01+\n");
        printf("  nrf24 send <hexdata>            transmit data\n");
        printf("  nrf24 listen                    enter RX mode\n");
        printf("  nrf24 status                    show registers\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        uint8_t csn = (argc >= 3) ? (uint8_t)atoi(argv[2]) : 8;
        uint8_t ce  = (argc >= 4) ? (uint8_t)atoi(argv[3]) : 9;
        nrf24_init(&s_nrf, csn, ce);
        if (nrf24_detect(&s_nrf)) {
            printf("NRF24L01+ detected\n");
        } else {
            printf("NRF24L01+ not found\n");
        }
    } else if (strcmp(argv[1], "send") == 0 && argc >= 3) {
        uint8_t buf[32];
        int len = 0;
        char *tok = strtok(argv[2], " ");
        while (tok && len < 32) {
            buf[len++] = (uint8_t)strtoul(tok, NULL, 16);
            tok = strtok(NULL, " ");
        }
        nrf24_tx_mode(&s_nrf);
        if (nrf24_send(&s_nrf, buf, len))
            printf("sent %d bytes\n", len);
        else
            printf("send failed\n");
    } else if (strcmp(argv[1], "listen") == 0) {
        nrf24_rx_mode(&s_nrf);
        printf("listening...\n");
        uint8_t buf[32], len;
        if (nrf24_available(&s_nrf) && nrf24_read(&s_nrf, buf, &len)) {
            printf("received %d bytes: ", len);
            for (int i = 0; i < len; i++) printf("%02X ", buf[i]);
            printf("\n");
        } else {
            printf("no data\n");
        }
    } else if (strcmp(argv[1], "status") == 0) {
        nrf24_print_regs(&s_nrf);
    } else printf("unknown nrf24: %s\n", argv[1]);
}

static module_cmd_t s_cmds[] = {
    {"nrf24", "NRF24L01+ radio (init/send/listen/status)", cmd_nrf24},
};

static bool mod_nrf24_load(void) {
    printf("nrf24: loaded\n");
    return true;
}

static void mod_nrf24_unload(void) {
    nrf24_deinit(&s_nrf);
    printf("nrf24: unloaded\n");
}

plugin_api_t MOD_NRF24 = {
    .init = mod_nrf24_load,
    .deinit = mod_nrf24_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
