#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "swarm.h"

static void cmd_swarm(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  swarm init             - start ESP-NOW mesh\n");
        printf("  swarm id <name>        - set this node's name\n");
        printf("  swarm mac              - show this node's MAC\n");
        printf("  swarm peer <MAC>       - register a peer node\n");
        printf("  swarm pub <lat> <lon> <alt> <hdg> <state>  - broadcast position\n");
        printf("  swarm list             - show all known peers\n");
        printf("  swarm stop             - stop the mesh\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (swarm_init()) printf("swarm: ESP-NOW mesh started\n");
        else printf("swarm: init FAILED\n");
    } else if (strcmp(argv[1], "id") == 0 && argc >= 3) {
        swarm_set_id(argv[2]);
        printf("swarm: id set to '%s'\n", argv[2]);
    } else if (strcmp(argv[1], "mac") == 0) {
        const char *mac = swarm_get_mac_str();
        if (mac && mac[0]) printf("swarm MAC: %s\n", mac);
        else printf("swarm: not initialised\n");
    } else if (strcmp(argv[1], "peer") == 0 && argc >= 3) {
        if (swarm_add_peer(argv[2])) printf("swarm: peer %s added\n", argv[2]);
        else printf("swarm: failed to add peer\n");
    } else if (strcmp(argv[1], "pub") == 0 && argc >= 7) {
        float lat = atof(argv[2]), lon = atof(argv[3]);
        float alt = atof(argv[4]), hdg = atof(argv[5]);
        int state = atoi(argv[6]);
        if (!swarm_is_active()) {
            printf("swarm: not initialised - run 'swarm init' first\n");
            return;
        }
        swarm_publish(lat, lon, alt, hdg, (uint8_t)state);
        printf("swarm: published position\n");
    } else if (strcmp(argv[1], "list") == 0) {
        if (!swarm_is_active()) { printf("swarm: not initialised\n"); return; }
        int n = swarm_peer_count();
        printf("swarm peers (%d):\n", n);
        for (int i = 0; i < n; i++) {
            const swarm_peer_t *p = swarm_get_peer(i);
            if (p) {
                printf("  %d. %02X:%02X:%02X:%02X:%02X:%02X  %s\n",
                       i + 1,
                       p->mac[0], p->mac[1], p->mac[2],
                       p->mac[3], p->mac[4], p->mac[5],
                       p->name);
                if (p->last_seen > 0)
                    printf("     lat=%.4f lon=%.4f alt=%.1f hdg=%.1f state=%d last_seen=%lu\n",
                           p->lat, p->lon, p->alt, p->hdg, p->state,
                           (unsigned long)p->last_seen);
            }
        }
    } else if (strcmp(argv[1], "stop") == 0) {
        swarm_stop();
        printf("swarm: stopped\n");
    } else printf("swarm: unknown subcommand '%s'\n", argv[1]);
}

static module_cmd_t s_cmds[] = {
    {"swarm", "ESP-NOW mesh (init/id/mac/peer/pub/list/stop)", cmd_swarm},
};

static bool mod_swarm_load(void) {
    if (!swarm_init()) {
        printf("swarm: init failed\n");
        return false;
    }
    printf("swarm: loaded\n");
    return true;
}

static void mod_swarm_unload(void) {
    swarm_stop();
    printf("swarm: unloaded\n");
}

plugin_api_t MOD_SWARM = {
    .init = mod_swarm_load,
    .deinit = mod_swarm_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
