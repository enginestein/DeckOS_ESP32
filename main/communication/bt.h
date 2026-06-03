#pragma once
#include <stdint.h>
#include <stdbool.h>

bool        bt_init(int baud);
void        bt_deinit(void);
void        bt_shell(void);
void        bt_exec(const char* cmd);
bool        bt_is_connected(void);
void        bt_printf(const char* fmt, ...);
bool        bt_log_is_enabled(void);
void        bt_log_mirror(const char* level, const char* tag, const char* msg, uint32_t ts);
void        bt_top_start(int ms);
void        bt_top_stop(void);
void        bt_send_file(const char* path);
void        bt_recv_file(const char* path);
