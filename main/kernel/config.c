#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "config.h"

static uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

static uint32_t config_crc(const flash_config_t* cfg) {
    size_t len = offsetof(flash_config_t, crc32);
    return crc32((const uint8_t*)cfg, len);
}

void config_defaults(flash_config_t* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic        = CONFIG_MAGIC;
    cfg->version      = CONFIG_VERSION;
    cfg->boot_cpu_mhz = 0;
    cfg->boot_led     = 0;
    cfg->shell_echo   = 1;
    strncpy(cfg->hostname, "esp32", sizeof(cfg->hostname) - 1);
    cfg->crc32        = config_crc(cfg);
}

bool config_load(flash_config_t* cfg) {
    config_defaults(cfg);

    uint32_t magic, version, crc;
    if (!hal_nvs_get_u32("cfg_magic", &magic) ||
        !hal_nvs_get_u32("cfg_ver", &version)) {
        printf("[config] no stored config - using defaults\n");
        return false;
    }
    if (magic != CONFIG_MAGIC || version != CONFIG_VERSION) {
        printf("[config] version mismatch - using defaults\n");
        return false;
    }

    uint32_t tmp32;
    char tmpstr[32];

    if (hal_nvs_get_u32("cfg_cpumhz", &tmp32)) cfg->boot_cpu_mhz = tmp32;
    if (hal_nvs_get_u32("cfg_led", &tmp32))    cfg->boot_led = (uint8_t)tmp32;
    if (hal_nvs_get_u32("cfg_echo", &tmp32))   cfg->shell_echo = (uint8_t)tmp32;
    if (hal_nvs_get_str("cfg_hostname", tmpstr, sizeof(tmpstr)))
        strncpy(cfg->hostname, tmpstr, sizeof(cfg->hostname) - 1);

    if (hal_nvs_get_u32("cfg_crc", &crc)) cfg->crc32 = crc;

    uint32_t expected = config_crc(cfg);
    if (cfg->crc32 != expected) {
        printf("[config] CRC mismatch - using defaults\n");
        config_defaults(cfg);
        return false;
    }
    return true;
}

void config_save(flash_config_t* cfg) {
    cfg->magic   = CONFIG_MAGIC;
    cfg->version = CONFIG_VERSION;
    cfg->crc32   = config_crc(cfg);

    hal_nvs_set_u32("cfg_magic",    cfg->magic);
    hal_nvs_set_u32("cfg_ver",      cfg->version);
    hal_nvs_set_u32("cfg_cpumhz",   cfg->boot_cpu_mhz);
    hal_nvs_set_u32("cfg_led",      cfg->boot_led);
    hal_nvs_set_u32("cfg_echo",     cfg->shell_echo);
    hal_nvs_set_str("cfg_hostname", cfg->hostname);
    hal_nvs_set_u32("cfg_crc",      cfg->crc32);
    hal_nvs_commit();

    printf("[config] saved to NVS\n");
}

void config_print(const flash_config_t* cfg) {
    printf("hostname    : %s\n",   cfg->hostname);
    printf("boot_cpu_mhz: %lu  (%s)\n",
           (unsigned long)cfg->boot_cpu_mhz,
           cfg->boot_cpu_mhz ? "custom" : "default 125 MHz");
    printf("boot_led    : %s\n",   cfg->boot_led    ? "on"  : "off");
    printf("shell_echo  : %s\n",   cfg->shell_echo  ? "yes" : "no");
    printf("crc32       : 0x%08lX\n", (unsigned long)cfg->crc32);
}
