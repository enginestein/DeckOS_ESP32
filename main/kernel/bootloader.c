#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "hal.h"
#include "bootloader.h"
#include "config.h"
#include "syslog.h"

flash_config_t g_config;


#define MAX_STAGES 24
static struct {
    const char *name;
    uint64_t    start_us;
    uint64_t    elapsed_us;
    bool        ok;
} s_stages[MAX_STAGES];
static int s_stage_count = 0;

static void stage_begin(const char *name) {
    if (s_stage_count >= MAX_STAGES) return;
    s_stages[s_stage_count].name      = name;
    s_stages[s_stage_count].start_us  = esp_timer_get_time();
    s_stages[s_stage_count].elapsed_us = 0;
    s_stages[s_stage_count].ok        = false;
    printf("  %-28s ... ", name);
    fflush(stdout);
}

static void stage_end(bool ok) {
    if (s_stage_count >= MAX_STAGES) return;
    s_stages[s_stage_count].elapsed_us = esp_timer_get_time() - s_stages[s_stage_count].start_us;
    s_stages[s_stage_count].ok         = ok;
    const char *mark = ok ? "[ OK ]" : "[FAIL]";
    uint64_t ms = s_stages[s_stage_count].elapsed_us / 1000;
    uint64_t us = s_stages[s_stage_count].elapsed_us % 1000;
    printf("%s  (%"PRIu64".%03"PRIu64" ms)\n", mark, ms, us);
    s_stage_count++;
}


static const char *boot_reason_str(void) {
    esp_reset_reason_t r = esp_reset_reason();
    switch (r) {
        case ESP_RST_UNKNOWN:   return "unknown";
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "external pin";
        case ESP_RST_SW:        return "software reset";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "task watchdog";
        case ESP_RST_WDT:       return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "?";
    }
}

static void print_chip_info(void) {
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    const char *model;
    switch (ci.model) {
        case CHIP_ESP32:    model = "ESP32";    break;
        case CHIP_ESP32S2:  model = "ESP32-S2";  break;
        case CHIP_ESP32S3:  model = "ESP32-S3";  break;
        case CHIP_ESP32C3:  model = "ESP32-C3";  break;
        case CHIP_ESP32H2:  model = "ESP32-H2";  break;
        case CHIP_ESP32C2:  model = "ESP32-C2";  break;
        case CHIP_ESP32C6:  model = "ESP32-C6";  break;
        default:            model = "ESP32 (unknown variant)";
    }
    printf("  model     : %s rev %d\n", model, ci.revision);
    printf("  cores     : %d\n", ci.cores);
    printf("  features  : %s%s%s%s\n",
           ci.features & CHIP_FEATURE_WIFI_BGN ? "WiFi " : "",
           ci.features & CHIP_FEATURE_BLE ? "BLE " : "",
           ci.features & CHIP_FEATURE_BT ? "BT " : "",
           ci.features & CHIP_FEATURE_EMB_FLASH ? "embedded-flash" : "");
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    printf("  flash     : %"PRIu32" KB", flash_size / 1024);
    uint32_t psram = hal_board_psram_size();
    if (psram) printf("  +  %"PRIu32" KB PSRAM", psram / 1024);
    printf("\n");
    printf("  reset     : %s\n", boot_reason_str());
}

static void print_inventory(void) {
    printf("\n  ── Hardware inventory ──\n");
    printf("  ADC   : 2 x 12-bit (GP26, GP27, GP28)\n");
    printf("  I2C   : bus 0 (GP21 SDA, GP22 SCL) @ 100 kHz\n");
    printf("  SPI   : SPI2 (GP18 SCK, GP23 MOSI, GP19 MISO)\n");
    printf("  UART  : UART1 (GP22 TX, GP23 RX)\n");
    if (hal_board_has_camera())
        printf("  CAM   : OV2640 present\n");
    if (hal_board_has_sdcard())
        printf("  SD    : card detected\n");
    printf("  PSRAM : %s\n", hal_board_psram_size() ? "enabled" : "none");
}


static void print_mem_summary(void) {
    printf("\n  ── Memory ──\n");
    printf("  DRAM total : %u KB\n",
           (unsigned)(heap_caps_get_total_size(MALLOC_CAP_8BIT) / 1024));
    printf("  DRAM free  : %u KB\n",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024));

    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_total) {
        printf("  PSRAM total: %u KB\n", (unsigned)(psram_total / 1024));
        printf("  PSRAM free : %u KB\n",
               (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    }
}

static void print_banner(void) {
    printf("\n");
    printf("  ╔══════════════════════════════════════════════╗\n");
    printf("  ║              DeckOS  v2.0                    ║\n");
    printf("  ║          ESP32 Port  (ESP-IDF)               ║\n");
    printf("  ║          Built %s %s              ║\n", __DATE__, __TIME__);
    printf("  ╚══════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  hostname  : %s\n", g_config.hostname[0] ? g_config.hostname : "(none set)");
    printf("\n");
}

void bootloader_run(void) {
    uint64_t boot_start = esp_timer_get_time();

    stage_begin("load config");
    bool had_valid = config_load(&g_config);
    if (!had_valid) {
        printf("[defaults] ");
        config_defaults(&g_config);
    }
    stage_end(true);

    stage_begin("detect board");
    hal_board_detect();
    stage_end(true);

    stage_begin("init flash");
    uint32_t flash_size = 0;
    bool flash_ok = (esp_flash_get_size(NULL, &flash_size) == ESP_OK && flash_size > 0);
    stage_end(flash_ok);

    stage_begin("init NVS");
    bool nvs_ok = hal_nvs_init();
    stage_end(nvs_ok);

    stage_begin("init SPIFFS");
    bool spiffs_ok = hal_spiffs_init();
    stage_end(spiffs_ok);

    stage_begin("init GPIO");
    hal_gpio_init(33);  // LED pin
    stage_end(true);

    stage_begin("init ADC");
    hal_adc_init();
    stage_end(true);

    stage_begin("init I2C");
    bool i2c_ok = hal_i2c_init(21, 22, 100000);
    stage_end(i2c_ok);

    stage_begin("init SPI");
    bool spi_ok = (hal_spi_init(18, 23, 19, 1000000) != NULL);
    stage_end(spi_ok);

    stage_begin("apply config");
    if (g_config.boot_led) {
        hal_gpio_set_dir(33, true);
        hal_gpio_put(33, true);
    }
    stage_end(true);

    uint64_t total_us = esp_timer_get_time() - boot_start;
    uint64_t total_ms = total_us / 1000;
    uint64_t rem_us   = total_us % 1000;

    printf("\n  ── SoC info ──\n");
    print_chip_info();

    print_inventory();
    print_mem_summary();

    print_banner();

    printf("  ── Boot stages (%d) ──\n", s_stage_count);
    for (int i = 0; i < s_stage_count; i++)
        printf("  [%s] %s  (%"PRIu64".%03"PRIu64" ms)\n",
               s_stages[i].ok ? "OK" : "!!",
               s_stages[i].name,
               s_stages[i].elapsed_us / 1000,
               s_stages[i].elapsed_us % 1000);

    printf("\n  Boot complete in %"PRIu64".%03"PRIu64" ms\n", total_ms, rem_us);
    printf("  Free DRAM : %u KB\n",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024));
    printf("\n");
}

void bootloader_print_banner(const flash_config_t *cfg) {
    (void)cfg;
    print_banner();
}

bool bootloader_check_recovery(void) {
    uint32_t recovery = 0;
    if (hal_nvs_get_u32("recovery", &recovery) && recovery) {
        hal_nvs_set_u32("recovery", 0);
        hal_nvs_commit();
        printf("\n  *** RECOVERY MODE ***\n\n");
        return true;
    }
    return false;
}
