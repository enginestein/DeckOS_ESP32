#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_ERROR,
} wifi_state_t;

typedef struct {
    uint8_t ssid[33];
    int8_t  rssi;
    uint8_t encryption;
} wifi_ap_t;

bool        wifi_init(void);
bool        wifi_ensure_core(void);
void        wifi_deinit(void);
bool        wifi_scan(wifi_ap_t* aps, int max, int* count);
bool        wifi_connect(const char* ssid, const char* pass);
void        wifi_disconnect(void);
wifi_state_t wifi_get_state(void);
char*       wifi_get_ip(void);
int8_t      wifi_get_rssi(void);
void        wifi_print_status(void);
bool        wifi_ap_start(const char* ssid, const char* pass);
void        wifi_ap_stop(void);
bool        wifi_http_get(const char* url, char* resp, size_t resp_size);
bool        wifi_http_post(const char* url, const char* body, char* resp, size_t resp_size);
bool        wifi_serve_start(uint16_t port);
void        wifi_serve_stop(void);
bool        wifi_telnet_start(uint16_t port);
void        wifi_telnet_stop(void);
