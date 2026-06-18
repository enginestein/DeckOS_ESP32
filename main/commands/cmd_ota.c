#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "hal.h"
#include "wifi.h"

#define OTA_BUF_SIZE 4096

// Parse scheme + host + path from a URL (very basic, no auth/port)
// Returns 0 on success, -1 on error.
static int parse_url(const char *url, char *host, int hostlen,
                     char *path, int pathlen, uint16_t *port) {
    const char *h = url;
    *port = 80;

    // Strip scheme
    if (strncmp(h, "http://", 7) == 0) h += 7;
    else if (strncmp(h, "https://", 8) == 0) { h += 8; *port = 443; }
    else { printf("ota: only http:// supported\n"); return -1; }

    // Find end of host
    const char *slash = strchr(h, '/');
    const char *colon = strchr(h, ':');
    int hlen;
    if (colon && (!slash || colon < slash)) {
        hlen = (int)(colon - h);
        if (hlen >= hostlen) hlen = hostlen - 1;
        strncpy(host, h, hlen); host[hlen] = '\0';
        *port = (uint16_t)atoi(colon + 1);
        h = slash ? slash : "";
    } else {
        hlen = slash ? (int)(slash - h) : (int)strlen(h);
        if (hlen >= hostlen) hlen = hostlen - 1;
        strncpy(host, h, hlen); host[hlen] = '\0';
        h = slash ? slash : "/";
    }

    if (!*h) h = "/";
    strncpy(path, h, pathlen - 1); path[pathlen - 1] = '\0';

    return 0;
}

void cmd_ota(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  ota http <url>          – flash firmware from HTTP URL, then reboot\n");
        printf("  ota status              – show OTA partition info\n");
        return;
    }

    if (!strcmp(argv[1], "status")) {
        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *next    = esp_ota_get_next_update_partition(NULL);
        esp_ota_img_states_t state;

        printf("running: %s @ 0x%08"PRIx32" (%"PRIu32" KB)\n",
               running->label, running->address, running->size / 1024);
        if (next)
            printf("next:    %s @ 0x%08"PRIx32" (%"PRIu32" KB)\n",
                   next->label, next->address, next->size / 1024);
        if (esp_ota_get_state_partition(running, &state) == ESP_OK)
            printf("state:   %d\n", state);
        return;
    }

    if (!strcmp(argv[1], "http")) {
        if (argc < 3) { printf("ota: missing URL\n"); return; }

        // Parse URL
        char host[128], path[256];
        uint16_t port;
        if (parse_url(argv[2], host, sizeof(host), path, sizeof(path), &port) < 0)
            return;

        if (wifi_get_state() != WIFI_CONNECTED) {
            printf("ota: WiFi not connected\n");
            return;
        }

        const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
        if (!update_part) { printf("ota: no OTA partition available\n"); return; }

        printf("ota: downloading %s -> %s @ 0x%"PRIx32"\n",
               argv[2], update_part->label, update_part->address);

        // Resolve hostname
        struct hostent *he = gethostbyname(host);
        if (!he) { printf("ota: DNS failed for %s\n", host); return; }

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) { printf("ota: socket failed\n"); return; }

        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port   = htons(port);
        memcpy(&dst.sin_addr, he->h_addr_list[0], he->h_length);

        struct timeval to = { .tv_sec = 30, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

        if (connect(sock, (struct sockaddr*)&dst, sizeof(dst)) < 0) {
            printf("ota: connect failed\n"); close(sock); return;
        }

        // Send HTTP request
        char req[512];
        int reqlen = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
            path, host);
        if (send(sock, req, reqlen, 0) != reqlen) {
            printf("ota: send failed\n"); close(sock); return;
        }

        // Read response headers, find Content-Length
        char hdr_buf[2048];
        int hdr_total = 0;
        int content_len = -1;
        bool headers_done = false;

        while (hdr_total < (int)sizeof(hdr_buf) - 1) {
            int n = recv(sock, hdr_buf + hdr_total, sizeof(hdr_buf) - 1 - hdr_total, 0);
            if (n <= 0) break;
            hdr_total += n;
            hdr_buf[hdr_total] = '\0';

            char *eoh = strstr(hdr_buf, "\r\n\r\n");
            if (eoh) {
                *eoh = '\0';
                // Parse headers
                char *line = hdr_buf;
                while (line && line < eoh) {
                    char *nl = strstr(line, "\r\n");
                    if (nl) *nl = '\0';
                    if (strncasecmp(line, "Content-Length:", 15) == 0) {
                        content_len = atoi(line + 15);
                    }
                    line = nl ? nl + 2 : NULL;
                }
                headers_done = true;
                break;
            }
        }

        if (!headers_done) { printf("ota: no response headers\n"); close(sock); return; }
        if (content_len <= 0) content_len = 32 * 1024 * 1024; // fallback: don't limit

        printf("ota: content-length: %d bytes\n", content_len);
        if (content_len > (int)update_part->size) {
            printf("ota: firmware too large for partition (%"PRIu32")\n", update_part->size);
            close(sock); return;
        }

        // Start OTA
        esp_ota_handle_t handle;
        esp_err_t err = esp_ota_begin(update_part, content_len, &handle);
        if (err != ESP_OK) { printf("ota: begin failed: %s\n", esp_err_to_name(err)); close(sock); return; }

        // Read body and write to OTA partition
        uint8_t *buf = (uint8_t *)malloc(OTA_BUF_SIZE);
        if (!buf) { printf("ota: out of memory\n"); close(sock); esp_ota_abort(handle); return; }

        int total = 0;
        int remaining = content_len;

        while (remaining > 0) {
            int chunk = (remaining > OTA_BUF_SIZE) ? OTA_BUF_SIZE : remaining;
            int n = recv(sock, buf, chunk, 0);
            if (n <= 0) break;

            err = esp_ota_write(handle, buf, n);
            if (err != ESP_OK) {
                printf("ota: write failed: %s\n", esp_err_to_name(err));
                free(buf); close(sock); esp_ota_abort(handle);
                return;
            }
            total += n;
            remaining -= n;
        }

        free(buf);
        close(sock);

        if (total < content_len) {
            printf("ota: got %d bytes, expected %d — aborting\n", total, content_len);
            esp_ota_abort(handle);
            return;
        }

        err = esp_ota_end(handle);
        if (err != ESP_OK) {
            printf("ota: end failed: %s\n", esp_err_to_name(err));
            return;
        }

        err = esp_ota_set_boot_partition(update_part);
        if (err != ESP_OK) {
            printf("ota: set boot partition failed: %s\n", esp_err_to_name(err));
            return;
        }

        printf("ota: success! rebooting in 1s... (%d bytes written to %s)\n",
               total, update_part->label);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    printf("ota: unknown subcommand '%s'\n", argv[1]);
}
