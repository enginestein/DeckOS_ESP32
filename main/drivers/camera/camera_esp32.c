#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "hal.h"
#include "camera_esp32.h"

static bool deck_cam_ready = false;
static camera_config_t cam_cfg;
static framesize_t cur_res = FRAMESIZE_VGA;
static cam_resolution_t cur_res_idx = CAM_RES_VGA;

static const struct {
    framesize_t fsize;
    uint16_t w, h;
} res_map[] = {
    { FRAMESIZE_QQVGA, 160, 120 },
    { FRAMESIZE_QVGA,  320, 240 },
    { FRAMESIZE_VGA,   640, 480 },
    { FRAMESIZE_SVGA,  800, 600 },
    { FRAMESIZE_XGA,   1024, 768 },
    { FRAMESIZE_SXGA,  1280, 1024 },
    { FRAMESIZE_UXGA,  1600, 1200 },
};

bool deck_cam_detect(void) {
    // Check if PSRAM is available (required for camera)
    if (hal_board_psram_size() == 0) return false;
    return true;
}

bool deck_cam_init(void) {
    if (deck_cam_ready) return true;

    // Default pin config for Robodo/AI-Thinker ESP32-CAM
    camera_config_t config = {
        .pin_pwdn  = 32,
        .pin_reset = -1,
        .pin_xclk = 0,
        .pin_sscb_sda = 26,
        .pin_sscb_scl = 27,
        .pin_d7 = 35,
        .pin_d6 = 34,
        .pin_d5 = 39,
        .pin_d4 = 36,
        .pin_d3 = 21,
        .pin_d2 = 19,
        .pin_d1 = 18,
        .pin_d0 = 5,
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 12,
        .fb_count = 2,
    };

    if (hal_board_psram_size() > 0) {
        config.fb_count = 2;
        config.frame_size = FRAMESIZE_SXGA;
    } else {
        config.fb_count = 1;
        config.frame_size = FRAMESIZE_CIF;
    }

    cam_cfg = config;

    esp_err_t err = esp_camera_init(&cam_cfg);
    if (err != ESP_OK) {
        printf("[camera] init failed: %s\n", esp_err_to_name(err));
        return false;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, cur_res);
        s->set_quality(s, 12);
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        s->set_special_effect(s, 0);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_gain_ctrl(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
    }

    deck_cam_ready = true;
    printf("[camera] ESP32-CAM ready\n");
    return true;
}

bool deck_cam_capture(cam_frame_t* frame) {
    if (!deck_cam_ready || !frame) return false;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;

    frame->buf    = fb->buf;
    frame->len    = fb->len;
    frame->width  = fb->width;
    frame->height = fb->height;
    frame->jpeg   = (fb->format == PIXFORMAT_JPEG);
    return true;
}

void deck_cam_return_frame(cam_frame_t* frame) {
    if (frame && frame->buf) {
        esp_camera_fb_return((camera_fb_t*)frame->buf);
        frame->buf = NULL;
    }
}

bool deck_cam_set_resolution(cam_resolution_t res) {
    if (res >= sizeof(res_map)/sizeof(res_map[0])) return false;
    cur_res_idx = res;
    cur_res = res_map[res].fsize;
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return false;
    return s->set_framesize(s, cur_res) == 0;
}

void deck_cam_set_quality(uint8_t q) {
    if (q < 10) q = 10;
    if (q > 63) q = 63;
    sensor_t* s = esp_camera_sensor_get();
    if (s) s->set_quality(s, q);
}

void deck_cam_set_brightness(int8_t v) {
    sensor_t* s = esp_camera_sensor_get();
    if (s) s->set_brightness(s, v);
}

void deck_cam_set_contrast(int8_t v) {
    sensor_t* s = esp_camera_sensor_get();
    if (s) s->set_contrast(s, v);
}

void deck_cam_deinit(void) {
    if (deck_cam_ready) {
        esp_camera_deinit();
        deck_cam_ready = false;
    }
}

bool deck_cam_is_ready(void) { return deck_cam_ready; }

static const char* res_name(cam_resolution_t r) {
    static const char* names[] = {"QQVGA","QVGA","VGA","SVGA","XGA","SXGA","UXGA"};
    if (r >= sizeof(names)/sizeof(names[0])) return "?";
    return names[r];
}

void deck_cam_print_info(void) {
    printf("Camera: OV2640/OV3660\n");
    sensor_t* s = esp_camera_sensor_get();
    if (!s) { printf("  sensor not available\n"); return; }
    printf("  resolution: %s (%dx%d)\n", res_name(cur_res_idx),
           res_map[cur_res_idx].w, res_map[cur_res_idx].h);
    printf("  pixel format: %s\n",
           cam_cfg.pixel_format == PIXFORMAT_JPEG ? "JPEG" : "RGB565");
    printf("  jpeg quality: %d\n", cam_cfg.jpeg_quality);
    printf("  frame buffer count: %d\n", cam_cfg.fb_count);
    printf("  PSRAM: %s\n", hal_board_psram_size() > 0 ? "yes" : "no");
}

static int save_index = 0;

bool deck_cam_save(const char* path) {
    if (!deck_cam_ready) return false;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;
    size_t len = fb->len;
    bool ok = hal_spiffs_write(path, fb->buf, len);
    esp_camera_fb_return(fb);
    if (ok) printf("saved %zu bytes to SPIFFS: %s\n", len, path);
    return ok;
}

bool deck_cam_save_auto(void) {
    char path[64];
    snprintf(path, sizeof(path), "/spiffs/capture_%d.jpg", save_index++);
    return deck_cam_save(path);
}

void deck_cam_list(void) {
    printf("Captures in SPIFFS (/spiffs/):\n");
    hal_spiffs_list("/spiffs");
}

// --- HTTP server (MJPEG stream + file listing + download) ---
static httpd_handle_t stream_httpd = NULL;

static esp_err_t index_handler(httpd_req_t *req) {
    const char* html =
        "<html><head><title>DeckOS ESP32-CAM</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;margin:20px}a{display:inline-block;margin:10px 0}</style>"
        "</head><body>"
        "<h1>DeckOS ESP32-CAM</h1>"
        "<a href='/stream'>Live MJPEG Stream</a><br>"
        "<a href='/list'>Saved Captures</a>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html);
    return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");

    while (stream_httpd) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) break;

        char part[256];
        int hlen = snprintf(part, sizeof(part),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
            fb->len);

        if (httpd_resp_send_chunk(req, part, hlen) != ESP_OK) {
            esp_camera_fb_return(fb); break;
        }
        if (httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK) {
            esp_camera_fb_return(fb); break;
        }
        esp_camera_fb_return(fb);
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t list_handler(httpd_req_t *req) {
    char buf[2048];
    int off = 0;
    off += snprintf(buf + off, sizeof(buf) - off,
        "<html><head><title>Saved Captures</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;margin:20px}li{margin:8px 0}</style>"
        "</head><body><h1>Saved Captures</h1><ul>");

    // Read SPIFFS dir via opendir/readdir
    DIR* d = opendir("/spiffs");
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            off += snprintf(buf + off, sizeof(buf) - off,
                "<li><a href='/capture/%s'>%s</a></li>",
                e->d_name, e->d_name);
            if (off >= (int)sizeof(buf) - 128) break;
        }
        closedir(d);
    }
    off += snprintf(buf + off, sizeof(buf) - off, "</ul><a href='/'>Back</a></body></html>");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t capture_handler(httpd_req_t *req) {
    // Extract filename from URI: /capture/foo.jpg
    const char* uri = req->uri + 9; // skip "/capture/"
    if (!uri || !*uri) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char path[64];
    snprintf(path, sizeof(path), "/spiffs/%s", uri);

    FILE* f = fopen(path, "rb");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); httpd_resp_send_404(req); return ESP_FAIL; }

    uint8_t* buf = malloc(len);
    if (!buf) { fclose(f); httpd_resp_send_500(req); return ESP_FAIL; }

    fread(buf, 1, len, f);
    fclose(f);

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, (const char*)buf, len);
    free(buf);
    return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

bool deck_cam_stream_start(void) {
    if (stream_httpd) return true;

    // Suppress noisy client-disconnect warnings from the HTTP server
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 5;
    cfg.lru_purge_enable = true;

    httpd_uri_t uris[] = {
        {"/",           HTTP_GET, index_handler,    NULL},
        {"/stream",     HTTP_GET, stream_handler,   NULL},
        {"/list",       HTTP_GET, list_handler,     NULL},
        {"/capture/*",  HTTP_GET, capture_handler,  NULL},
        {"/favicon.ico", HTTP_GET, favicon_handler, NULL},
    };

    if (httpd_start(&stream_httpd, &cfg) != ESP_OK) return false;
    for (int i = 0; i < 5; i++) {
        if (httpd_register_uri_handler(stream_httpd, &uris[i]) != ESP_OK) {
            httpd_stop(stream_httpd);
            stream_httpd = NULL;
            return false;
        }
    }
    printf("[camera] HTTP server started on port 80\n");
    return true;
}

void deck_cam_stream_stop(void) {
    if (stream_httpd) {
        httpd_stop(stream_httpd);
        stream_httpd = NULL;
    }
}

bool deck_cam_stream_is_active(void) {
    return stream_httpd != NULL;
}
