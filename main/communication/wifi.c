#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "hal.h"
#include "wifi.h"

#define WIFI_CONNECT_BIT  BIT0
#define WIFI_FAIL_BIT     BIT1

static EventGroupHandle_t wifi_event_group;
static wifi_state_t wifi_state = WIFI_DISCONNECTED;
static char wifi_ip[16] = "";
static int8_t wifi_rssi = 0;
static esp_netif_t* netif = NULL;
static bool wifi_inited = false;

static void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_state = WIFI_DISCONNECTED;
        wifi_ip[0] = '\0';
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        // client connected to AP — nothing special to do
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* ev = (ip_event_got_ip_t*)data;
        snprintf(wifi_ip, sizeof(wifi_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        wifi_rssi = 0;
        wifi_state = WIFI_CONNECTED;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECT_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_AP_STAIPASSIGNED) {
        // AP client got IP — grab AP interface IP
        esp_netif_ip_info_t ip;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip);
        snprintf(wifi_ip, sizeof(wifi_ip), IPSTR, IP2STR(&ip.ip));
    }
}

bool wifi_init(void) {
    if (wifi_inited) return true;

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif = esp_netif_create_default_wifi_sta();
    assert(netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_inited = true;
    printf("[wifi] ESP32 native WiFi ready\n");
    return true;
}

bool wifi_scan(wifi_ap_t* aps, int max, int* count) {
    if (!wifi_inited) return false;

    uint16_t num = max;
    wifi_ap_record_t* records = malloc(num * sizeof(wifi_ap_record_t));
    if (!records) return false;

    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&num, records);
    *count = (num < max) ? num : max;

    for (int i = 0; i < *count; i++) {
        memcpy(aps[i].ssid, records[i].ssid, 32);
        aps[i].ssid[32] = '\0';
        aps[i].rssi = records[i].rssi;
        aps[i].encryption = records[i].authmode;
    }

    free(records);
    return true;
}

bool wifi_connect(const char* ssid, const char* pass) {
    if (!wifi_inited) return false;

    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    if (pass) strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);

    wifi_state = WIFI_CONNECTING;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_CONNECT_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, 15000 / portTICK_PERIOD_MS);

    if (bits & WIFI_CONNECT_BIT) return true;
    wifi_state = WIFI_DISCONNECTED;
    return false;
}

void wifi_disconnect(void) {
    esp_wifi_disconnect();
    wifi_state = WIFI_DISCONNECTED;
}

wifi_state_t wifi_get_state(void) { return wifi_state; }
char* wifi_get_ip(void) { return wifi_ip; }
int8_t wifi_get_rssi(void) { return wifi_rssi; }

void wifi_print_status(void) {
    printf("WiFi status:\n");
    printf("  state: %s\n",
        wifi_state == WIFI_CONNECTED ? "connected" :
        wifi_state == WIFI_CONNECTING ? "connecting" :
        wifi_state == WIFI_DISCONNECTED ? "disconnected" : "error");
    if (wifi_state == WIFI_CONNECTED) {
        printf("  IP: %s\n", wifi_ip);
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            printf("  SSID: %s\n", ap.ssid);
            printf("  RSSI: %d dBm\n", ap.rssi);
        }
    }
}

bool wifi_http_get(const char* url, char* resp, size_t resp_size) {
    if (wifi_state != WIFI_CONNECTED) return false;

    char host[128], path[256];
    int port = 80;
    // Parse URL: http://host:port/path
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        snprintf(host, sizeof(host), "%.*s", (int)(colon - p), p);
        port = atoi(colon + 1);
    } else {
        int hostlen = slash ? (int)(slash - p) : (int)strlen(p);
        snprintf(host, sizeof(host), "%.*s", hostlen, p);
    }
    snprintf(path, sizeof(path), "%s", slash ? slash : "/");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct hostent* h = gethostbyname(host);
    if (!h) { close(sock); return false; }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    memcpy(&dest.sin_addr, h->h_addr, h->h_length);

    if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        close(sock); return false;
    }

    char req[512];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(sock, req, strlen(req), 0);

    int total = 0;
    int n;
    while (total < (int)resp_size - 1 &&
           (n = recv(sock, resp + total, resp_size - 1 - total, 0)) > 0) {
        total += n;
    }
    resp[total] = '\0';
    close(sock);

    // Skip HTTP headers
    char* body = strstr(resp, "\r\n\r\n");
    if (body) {
        body += 4;
        memmove(resp, body, total - (body - resp) + 1);
    }
    return true;
}

bool wifi_http_post(const char* url, const char* body, char* resp, size_t resp_size) {
    if (wifi_state != WIFI_CONNECTED) return false;

    char host[128], path[256];
    int port = 80;
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        snprintf(host, sizeof(host), "%.*s", (int)(colon - p), p);
        port = atoi(colon + 1);
    } else {
        int hostlen = slash ? (int)(slash - p) : (int)strlen(p);
        snprintf(host, sizeof(host), "%.*s", hostlen, p);
    }
    snprintf(path, sizeof(path), "%s", slash ? slash : "/");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct hostent* h = gethostbyname(host);
    if (!h) { close(sock); return false; }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    memcpy(&dest.sin_addr, h->h_addr, h->h_length);

    if (connect(sock, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        close(sock); return false;
    }

    char req[1024];
    int blen = strlen(body);
    snprintf(req, sizeof(req),
        "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
        path, host, blen, body);
    send(sock, req, strlen(req), 0);

    int total = 0, n;
    while (total < (int)resp_size - 1 &&
           (n = recv(sock, resp + total, resp_size - 1 - total, 0)) > 0) {
        total += n;
    }
    resp[total] = '\0';
    close(sock);

    char* b = strstr(resp, "\r\n\r\n");
    if (b) { b += 4; memmove(resp, b, total - (b - resp) + 1); }
    return true;
}

// --- SoftAP mode ---
static esp_netif_t* ap_netif = NULL;

bool wifi_ap_start(const char* ssid, const char* pass) {
    if (!wifi_inited) return false;

    if (ap_netif) {
        printf("[wifi] AP already running\n");
        return true;
    }

    ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    wifi_config_t cfg = {0};
    strncpy((char*)cfg.ap.ssid, ssid, sizeof(cfg.ap.ssid) - 1);
    cfg.ap.ssid_len = (uint8_t)strlen(ssid);
    cfg.ap.max_connection = 4;
    cfg.ap.authmode = (pass && strlen(pass) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    if (cfg.ap.authmode != WIFI_AUTH_OPEN) {
        strncpy((char*)cfg.ap.password, pass, sizeof(cfg.ap.password) - 1);
    }

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &cfg);

    esp_netif_ip_info_t ip = {0};
    IP4_ADDR(&ip.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip);
    esp_netif_dhcps_start(ap_netif);

    snprintf(wifi_ip, sizeof(wifi_ip), "192.168.4.1");
    printf("[wifi] AP started: %s (IP: 192.168.4.1)\n", ssid);
    return true;
}

void wifi_ap_stop(void) {
    if (ap_netif) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
        printf("[wifi] AP stopped\n");
    }
}

bool wifi_serve_start(uint16_t port) { return false; }
void wifi_serve_stop(void) {}
bool wifi_telnet_start(uint16_t port) { return false; }
void wifi_telnet_stop(void) {}
