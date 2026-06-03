#pragma once
#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAGIC   0x44454F53
#define CONFIG_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t boot_cpu_mhz;
    uint8_t  boot_led;
    uint8_t  shell_echo;
    char     hostname[32];
    uint32_t crc32;
} flash_config_t;

void config_defaults(flash_config_t* cfg);
bool config_load(flash_config_t* cfg);
void config_save(flash_config_t* cfg);
void config_print(const flash_config_t* cfg);
