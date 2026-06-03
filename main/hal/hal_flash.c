#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_spiffs.h"
#include "hal.h"

static nvs_handle_t s_nvs;
static bool nvs_opened = false;

bool hal_nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return false;
    err = nvs_open("DeckOS", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) return false;
    nvs_opened = true;
    return true;
}

bool hal_nvs_get_str(const char* key, char* buf, size_t bufsize) {
    if (!nvs_opened) return false;
    size_t len = bufsize;
    return nvs_get_str(s_nvs, key, buf, &len) == ESP_OK;
}

bool hal_nvs_set_str(const char* key, const char* val) {
    if (!nvs_opened) return false;
    return nvs_set_str(s_nvs, key, val) == ESP_OK;
}

bool hal_nvs_get_u32(const char* key, uint32_t* val) {
    if (!nvs_opened) return false;
    return nvs_get_u32(s_nvs, key, val) == ESP_OK;
}

bool hal_nvs_set_u32(const char* key, uint32_t val) {
    if (!nvs_opened) return false;
    return nvs_set_u32(s_nvs, key, val) == ESP_OK;
}

bool hal_nvs_get_blob(const char* key, uint8_t* buf, size_t* len) {
    if (!nvs_opened) return false;
    return nvs_get_blob(s_nvs, key, buf, len) == ESP_OK;
}

bool hal_nvs_set_blob(const char* key, const uint8_t* buf, size_t len) {
    if (!nvs_opened) return false;
    return nvs_set_blob(s_nvs, key, buf, len) == ESP_OK;
}

bool hal_nvs_commit(void) {
    if (!nvs_opened) return false;
    return nvs_commit(s_nvs) == ESP_OK;
}

void hal_nvs_erase_all(void) {
    if (nvs_opened) {
        nvs_erase_all(s_nvs);
        nvs_commit(s_nvs);
    }
}

bool hal_spiffs_init(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) return false;

    size_t total, used;
    esp_spiffs_info("spiffs", &total, &used);
    printf("[spiffs] mounted: %d total, %d used\n", total, used);
    return true;
}

bool hal_spiffs_read(const char* path, uint8_t* buf, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t r = fread(buf, 1, *len, f);
    *len = r;
    fclose(f);
    return true;
}

bool hal_spiffs_write(const char* path, const uint8_t* buf, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    return w == len;
}

bool hal_spiffs_delete(const char* path) {
    return remove(path) == 0;
}

bool hal_spiffs_stat(const char* path, size_t* out_size) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    *out_size = (size_t)st.st_size;
    return true;
}

void hal_spiffs_list(const char* dir) {
    DIR* d = opendir(dir);
    if (!d) { printf("[spiffs] cannot open %s\n", dir); return; }
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        printf("  %s\n", e->d_name);
    }
    closedir(d);
}
