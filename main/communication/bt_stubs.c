#include <stdio.h>
#include "bt.h"

bool bt_init(int baud) { (void)baud; return false; }
void bt_deinit(void) {}
bool bt_is_connected(void) { return false; }
void bt_shell(void) { printf("[bt] not available\n"); }
void bt_exec(const char* cmd) { (void)cmd; }
void bt_printf(const char* fmt, ...) { (void)fmt; }
bool bt_log_is_enabled(void) { return false; }
void bt_log_mirror(const char* level, const char* tag, const char* msg, uint32_t ts) { (void)level; (void)tag; (void)msg; (void)ts; }
void bt_top_start(int ms) { (void)ms; }
void bt_top_stop(void) {}
void bt_send_file(const char* path) { (void)path; }
void bt_recv_file(const char* path) { (void)path; }
