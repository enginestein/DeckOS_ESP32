#pragma once
#include <stdint.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERR   = 3,
} log_level_t;

void syslog_init(void);
void syslog_write(log_level_t lvl, const char* tag, const char* msg);
void syslog_dump(log_level_t min_level, int tail);
void syslog_clear(void);
uint32_t syslog_total(void);

uint32_t syslog_lock(void);
void     syslog_unlock(uint32_t saved);

#define LOG_D(tag, msg) syslog_write(LOG_DEBUG, tag, msg)
#define LOG_I(tag, msg) syslog_write(LOG_INFO,  tag, msg)
#define LOG_W(tag, msg) syslog_write(LOG_WARN,  tag, msg)
#define LOG_E(tag, msg) syslog_write(LOG_ERR,   tag, msg)
