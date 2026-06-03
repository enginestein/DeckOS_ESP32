#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal.h"
#include "swarm.h"

#define TAG "swarm"

static bool s_active = false;
static char s_node_name[SWARM_NAME_LEN] = "deck-node";
static char s_mac_str[SWARM_MAC_STR_LEN] = "";
static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static swarm_peer_t s_peers[SWARM_PEER_MAX];
static int s_peer_count = 0;

typedef struct __attribute__((packed)) {
    uint8_t type;
    char name[SWARM_NAME_LEN];
    float lat;
    float lon;
    float alt;
    float hdg;
    uint8_t state;
} swarm_packet_t;

#define SWARM_PKT_TELEM 0x01
#define SWARM_PKT_PEER  0x02

static int peer_find(const uint8_t *mac) {
    for (int i = 0; i < s_peer_count; i++)
        if (memcmp(s_peers[i].mac, mac, 6) == 0)
            return i;
    return -1;
}

static void peer_update(const uint8_t *mac, const char *name, float lat, float lon, float alt, float hdg, uint8_t state) {
    int idx = peer_find(mac);
    if (idx < 0) {
        if (s_peer_count >= SWARM_PEER_MAX) return;
        idx = s_peer_count++;
        memcpy(s_peers[idx].mac, mac, 6);
    }
    if (name) {
        strncpy(s_peers[idx].name, name, SWARM_NAME_LEN - 1);
        s_peers[idx].name[SWARM_NAME_LEN - 1] = '\0';
    }
    s_peers[idx].lat = lat;
    s_peers[idx].lon = lon;
    s_peers[idx].alt = alt;
    s_peers[idx].hdg = hdg;
    s_peers[idx].state = state;
    s_peers[idx].last_seen = (uint32_t)(hal_time_us() / 1000000);
}

static void swarm_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!data || len < 1) return;

    swarm_packet_t *pkt = (swarm_packet_t *)data;

    if (pkt->type == SWARM_PKT_TELEM && len >= sizeof(swarm_packet_t)) {
        peer_update(info->src_addr, pkt->name, pkt->lat, pkt->lon, pkt->alt, pkt->hdg, pkt->state);
        printf("[swarm] telemetry from %s\n", pkt->name);
    }
}

static void swarm_send_cb(const uint8_t *mac, esp_now_send_status_t status) {
}

bool swarm_init(void) {
    if (s_active) return true;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(swarm_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(swarm_send_cb));

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_mac_str, sizeof(s_mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_broadcast_mac, 6);
    peer.channel = 1;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    s_active = true;
    printf("[swarm] ESP-NOW mesh started (MAC: %s)\n", s_mac_str);
    return true;
}

void swarm_stop(void) {
    if (!s_active) return;
    esp_now_deinit();
    s_active = false;
    s_peer_count = 0;
    printf("[swarm] mesh stopped\n");
}

bool swarm_set_id(const char *name) {
    if (!name) return false;
    strncpy(s_node_name, name, SWARM_NAME_LEN - 1);
    s_node_name[SWARM_NAME_LEN - 1] = '\0';
    printf("[swarm] node id: %s\n", s_node_name);
    return true;
}

const char *swarm_get_id(void) {
    return s_node_name;
}

const char *swarm_get_mac_str(void) {
    return s_mac_str;
}

static bool parse_mac(const char *str, uint8_t *mac) {
    int vals[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x",
               &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++) mac[i] = (uint8_t)vals[i];
    return true;
}

bool swarm_add_peer(const char *mac_str) {
    if (!mac_str) return false;
    uint8_t mac[6];
    if (!parse_mac(mac_str, mac)) {
        printf("[swarm] invalid MAC: %s\n", mac_str);
        return false;
    }

    if (peer_find(mac) >= 0) {
        printf("[swarm] peer already added\n");
        return true;
    }

    if (s_peer_count >= SWARM_PEER_MAX) {
        printf("[swarm] peer table full\n");
        return false;
    }

    memcpy(s_peers[s_peer_count].mac, mac, 6);
    snprintf(s_peers[s_peer_count].name, SWARM_NAME_LEN, "peer-%d", s_peer_count);
    s_peers[s_peer_count].last_seen = 0;
    s_peer_count++;

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 1;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    printf("[swarm] peer added: %s\n", mac_str);
    return true;
}

void swarm_remove_peer(const char *mac_str) {
    uint8_t mac[6];
    if (!parse_mac(mac_str, mac)) return;
    int idx = peer_find(mac);
    if (idx < 0) return;

    esp_now_del_peer(mac);
    if (idx < s_peer_count - 1)
        s_peers[idx] = s_peers[s_peer_count - 1];
    s_peer_count--;
    printf("[swarm] peer removed: %s\n", mac_str);
}

void swarm_publish(float lat, float lon, float alt, float hdg, uint8_t state) {
    if (!s_active) return;

    swarm_packet_t pkt;
    pkt.type = SWARM_PKT_TELEM;
    strncpy(pkt.name, s_node_name, SWARM_NAME_LEN - 1);
    pkt.name[SWARM_NAME_LEN - 1] = '\0';
    pkt.lat = lat;
    pkt.lon = lon;
    pkt.alt = alt;
    pkt.hdg = hdg;
    pkt.state = state;

    esp_now_send(s_broadcast_mac, (uint8_t *)&pkt, sizeof(pkt));
    printf("[swarm] published: %s @ (%.4f, %.4f) alt=%.1f hdg=%.1f state=%d\n",
           s_node_name, lat, lon, alt, hdg, state);
}

int swarm_peer_count(void) {
    return s_peer_count;
}

const swarm_peer_t *swarm_get_peer(int idx) {
    if (idx < 0 || idx >= s_peer_count) return NULL;
    return &s_peers[idx];
}

void swarm_poll(void) {
}

bool swarm_is_active(void) {
    return s_active;
}
