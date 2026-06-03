#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal.h"
#include "file_persist.h"

#define FP_DIR "/persist"

bool file_persist_save(const char* name, const uint8_t* data, uint32_t len) {
    if (!name || !data) return false;
    char path[64];
    snprintf(path, sizeof(path), FP_DIR "/%s", name);
    return hal_spiffs_write(path, data, len);
}

bool file_persist_load(const char* name, uint8_t* buf, uint32_t* len, uint32_t max_len) {
    if (!name || !buf || !len) return false;
    char path[64];
    snprintf(path, sizeof(path), FP_DIR "/%s", name);

    size_t sz = max_len;
    if (!hal_spiffs_read(path, buf, &sz)) return false;
    *len = (uint32_t)sz;
    return true;
}

bool file_persist_delete(const char* name) {
    if (!name) return false;
    char path[64];
    snprintf(path, sizeof(path), FP_DIR "/%s", name);
    return hal_spiffs_delete(path);
}

void file_persist_list(void) {
    printf("persisted files:\n");
    hal_spiffs_list(FP_DIR);
}
