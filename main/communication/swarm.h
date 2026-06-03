#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SWARM_PEER_MAX 10
#define SWARM_NAME_LEN 32
#define SWARM_MAC_STR_LEN 18

typedef struct {
    uint8_t mac[6];
    char name[SWARM_NAME_LEN];
    float lat;
    float lon;
    float alt;
    float hdg;
    uint8_t state;
    uint32_t last_seen;
} swarm_peer_t;

bool        swarm_init(void);
void        swarm_stop(void);
bool        swarm_set_id(const char *name);
const char *swarm_get_id(void);
const char *swarm_get_mac_str(void);
bool        swarm_add_peer(const char *mac_str);
void        swarm_remove_peer(const char *mac_str);
void        swarm_publish(float lat, float lon, float alt, float hdg, uint8_t state);
int         swarm_peer_count(void);
const swarm_peer_t *swarm_get_peer(int idx);
void        swarm_poll(void);
bool        swarm_is_active(void);
