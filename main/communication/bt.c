#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "hal.h"
#include "bt.h"
#include "commands.h"

#define SPP_SERVER_NAME "DeckOS-SPP"
static uint32_t spp_handle = 0;
static bool bt_connected = false;
static bool bt_inited = false;
static bool bt_log_enabled = false;
static volatile bool bt_shell_active = false;

static void spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    switch (event) {
        case ESP_SPP_SRV_OPEN_EVT:
            bt_connected = true;
            printf("[bt] client connected\n");
            break;
        case ESP_SPP_CLOSE_EVT:
            bt_connected = false;
            bt_shell_active = false;
            printf("[bt] client disconnected\n");
            break;
        case ESP_SPP_DATA_IND_EVT:
            if (bt_shell_active && param->data_ind.len > 0) {
                int len = param->data_ind.len;
                if (len > 127) len = 127;
                const char* data = (const char*)param->data_ind.data;
                char buf[128];
                memcpy(buf, data, len);
                buf[len] = '\0';
                // Feed to shell command execution
                if (buf[0] == '\r' || buf[0] == '\n') return;
                char* nl = strchr(buf, '\r');
                if (nl) *nl = '\0';
                nl = strchr(buf, '\n');
                if (nl) *nl = '\0';
                if (strlen(buf) > 0) {
                    commands_execute(buf);
                    esp_spp_write(param->data_ind.handle, 4, (uint8_t*)"\r\n> ");
                }
            }
            break;
        default:
            break;
    }
}

bool bt_init(int baud) {
    (void)baud;

    esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (err != ESP_OK) return false;

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&cfg);
    if (err != ESP_OK) return false;

    err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) return false;

    err = esp_bluedroid_init();
    if (err != ESP_OK) return false;

    err = esp_bluedroid_enable();
    if (err != ESP_OK) return false;

    err = esp_spp_register_callback(spp_cb);
    if (err != ESP_OK) return false;

    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
    };
    err = esp_spp_enhanced_init(&spp_cfg);
    if (err != ESP_OK) return false;

    err = esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE,
                            0, SPP_SERVER_NAME);
    if (err != ESP_OK) return false;

    bt_inited = true;
    printf("[bt] ESP32 Bluetooth ready (SPP)\n");
    printf("[bt] name: %s\n", esp_bt_dev_get_address() ? "DeckOS" : "?");
    const uint8_t* mac = esp_bt_dev_get_address();
    if (mac) {
        printf("[bt] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return true;
}

void bt_deinit(void) {
    if (!bt_inited) return;
    esp_spp_stop_srv();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    bt_inited = false;
    bt_connected = false;
}

void bt_shell(void) {
    if (!bt_connected) {
        printf("[bt] no client connected\n");
        return;
    }
    bt_shell_active = true;
    printf("[bt] BT shell active (connect from phone)\n");
    while (bt_shell_active && bt_connected) {
        hal_sleep_ms(100);
    }
}

void bt_exec(const char* cmd) {
    if (!bt_connected) return;
    char buf[512];
    snprintf(buf, sizeof(buf), "\r\n> %s\r\n", cmd);
    esp_spp_write(spp_handle, strlen(buf), (uint8_t*)buf);
    // Execute and capture would need pipe - simplified
    commands_execute(cmd);
}

bool bt_is_connected(void) { return bt_connected; }

void bt_printf(const char* fmt, ...) {
    if (!bt_connected) return;
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    esp_spp_write(spp_handle, strlen(buf), (uint8_t*)buf);
}

bool bt_log_is_enabled(void) { return bt_log_enabled; }

void bt_log_mirror(const char* level, const char* tag, const char* msg, uint32_t ts) {
    if (!bt_connected || !bt_log_enabled) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "[%s] [%s] %s\r\n", level, tag, msg);
    esp_spp_write(spp_handle, strlen(buf), (uint8_t*)buf);
}

void bt_top_start(int ms) { (void)ms; printf("[bt] top not yet implemented\n"); }
void bt_top_stop(void) {}
void bt_send_file(const char* path) { (void)path; printf("[bt] file send not yet implemented\n"); }
void bt_recv_file(const char* path) { (void)path; printf("[bt] file recv not yet implemented\n"); }
