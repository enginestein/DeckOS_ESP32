#pragma once
#include <stdbool.h>

bool file_persist_save(const char* name, const uint8_t* data, uint32_t len);
bool file_persist_load(const char* name, uint8_t* buf, uint32_t* len, uint32_t max_len);
bool file_persist_delete(const char* name);
void file_persist_list(void);
