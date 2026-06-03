#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal.h"
#include "uart_detect.h"

static const uint32_t COMMON_BAUDS[] = {
    9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200
};
static const int BAUD_COUNT = (int)(sizeof(COMMON_BAUDS) / sizeof(COMMON_BAUDS[0]));

typedef struct {
    const char* name;
    const char* probe;
    const char* response;
    uint32_t baud;
} uart_device_t;

static const uart_device_t KNOWN_DEVICES[] = {
    {"HC-05 Bluetooth",  "AT\r\n", "OK", 9600},
    {"HC-05 Bluetooth",  "AT\r\n", "OK", 38400},
    {"HC-06 Bluetooth",  "AT",     "OK", 9600},
    {"ESP8266 WiFi",     "AT\r\n", "OK", 115200},
    {"ESP8266 WiFi",     "AT\r\n", "OK", 9600},
    {"SIM800 GSM",       "AT\r\n", "OK", 9600},
    {"GPS NMEA",         NULL,     "$GP", 9600},
    {"GPS NMEA",         NULL,     "$GP", 4800},
    {"Arduino",          NULL,     NULL, 9600},
};
static const int DEVICE_COUNT = (int)(sizeof(KNOWN_DEVICES) / sizeof(KNOWN_DEVICES[0]));

static uint32_t estimate_baud_from_pulses(uint8_t pin, uint32_t timeout_ms) {
    (void)DEVICE_COUNT;
    hal_gpio_set_dir(pin, false);
    hal_gpio_set_pull(pin, true, false);

    int initial = hal_gpio_get(pin);
    uint32_t start = hal_time_ms();

    while (hal_gpio_get(pin) == initial) {
        if ((hal_time_ms() - start) > timeout_ms) return 0;
        hal_sleep_us(10);
    }

    uint32_t widths[64];
    int wcount = 0;
    int last = hal_gpio_get(pin);
    uint64_t t_last = hal_time_us();

    uint32_t measure_end = hal_time_ms() + 50;
    while (hal_time_ms() < measure_end && wcount < 64) {
        int cur = hal_gpio_get(pin);
        if (cur != last) {
            uint64_t now = hal_time_us();
            uint32_t width = (uint32_t)(now - t_last);
            if (width > 1 && width < 100000) widths[wcount++] = width;
            t_last = now;
            last = cur;
        }
        hal_sleep_us(1);
    }

    if (wcount == 0) return 0;

    uint32_t min_width = 0xFFFFFFFF;
    for (int i = 0; i < wcount; i++)
        if (widths[i] < min_width) min_width = widths[i];

    if (min_width == 0) return 0;
    uint32_t raw_baud = 1000000 / min_width;

    uint32_t best = COMMON_BAUDS[0];
    uint32_t best_err = (raw_baud > best) ? raw_baud - best : best - raw_baud;
    for (int i = 1; i < BAUD_COUNT; i++) {
        uint32_t err = (raw_baud > COMMON_BAUDS[i]) ?
            raw_baud - COMMON_BAUDS[i] : COMMON_BAUDS[i] - raw_baud;
        if (err < best_err) { best_err = err; best = COMMON_BAUDS[i]; }
    }
    return best;
}

static const char* probe_device(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud) {
    hal_uart_init(tx_pin, rx_pin, baud, NULL);
    hal_sleep_ms(50);

    static char resp[128];
    const char* matched = NULL;

    for (int d = 0; d < DEVICE_COUNT && !matched; d++) {
        const uart_device_t* dev = &KNOWN_DEVICES[d];
        if (dev->baud != baud) continue;
        if (!dev->response) continue;

        // flush pending
        int pos = 0;
        while ((pos = hal_uart_read_char()) >= 0);
        pos = 0;

        if (dev->probe) {
            for (const char* p = dev->probe; *p; p++)
                hal_uart_write_char(*p);
        }

        uint32_t t0 = hal_time_ms();
        while ((hal_time_ms() - t0) < 800 && pos < (int)sizeof(resp) - 1) {
            int c = hal_uart_read_char();
            if (c >= 0) resp[pos++] = (char)c;
            hal_sleep_us(200);
        }
        resp[pos] = '\0';

        if (pos > 0 && strstr(resp, dev->response)) matched = dev->name;
    }

    if (!matched) {
        int pos = 0;
        uint32_t t0 = hal_time_ms();
        while ((hal_time_ms() - t0) < 1200 && pos < (int)sizeof(resp) - 1) {
            int c = hal_uart_read_char();
            if (c >= 0) {
                resp[pos++] = (char)c;
                resp[pos] = '\0';
                if (strstr(resp, "$GP") || strstr(resp, "$GN")) {
                    matched = "GPS (NMEA)";
                    break;
                }
            }
            hal_sleep_us(200);
        }
    }
    return matched;
}

void uart_detect_run(uint8_t rx_pin, uint32_t timeout_ms) {
    printf("=== UART auto-detect on GP%d ===\n", rx_pin);
    printf("  step 1: measuring pulse widths to estimate baud...\n");

    uint32_t t = timeout_ms ? timeout_ms : 3000;
    uint32_t baud = estimate_baud_from_pulses(rx_pin, t);

    if (baud == 0) {
        printf("  no activity detected on GP%d within %lu ms\n", rx_pin, t);
        printf("  tips: check wiring, ensure remote device is transmitting\n");
        return;
    }

    printf("  estimated baud : ~%lu\n", baud);

    hal_gpio_set_dir(rx_pin, false);
    hal_gpio_set_pull(rx_pin, true, false);
    int idle = hal_gpio_get(rx_pin);
    printf("  RX idle state  : %s (%s for UART)\n",
           idle ? "HIGH" : "LOW",
           idle ? "normal" : "inverted/RS232?");

    uint8_t tx_pin = (rx_pin % 2 == 1) ? rx_pin - 1 : rx_pin + 1;

    printf("  step 2: probing for known devices at %lu baud...\n", baud);

    const char* device = probe_device(tx_pin, rx_pin, baud);

    printf("\n  RESULT:\n");
    printf("  +------------------------------+\n");
    printf("  | pin      : GP%d              |\n", rx_pin);
    printf("  | baud     : %lu              |\n", baud);
    printf("  | idle     : %s              |\n", idle ? "HIGH" : "LOW");
    printf("  | device   : %s              |\n", device ? device : "unknown");
    printf("  | uart     : UART0 (TX=GP%d RX=GP%d)\n", tx_pin, rx_pin);
    printf("  +------------------------------+\n\n");

    if (device)
        printf("  Suggestion: uart %lu %d %d\n", baud, tx_pin, rx_pin);
}

void la_detect_protocol(uint8_t pin, int samples, int us_per_sample) {
    if (samples < 16 || samples > 512) samples = 256;
    if (us_per_sample < 1 || us_per_sample > 10000) us_per_sample = 5;

    printf("=== LA protocol detect on GP%d ===\n", pin);
    printf("  %d samples @ %d us/sample  (%.2f ms window)\n",
           samples, us_per_sample,
           (float)(samples * us_per_sample) / 1000.0f);
    printf("  sampling");

    uint8_t* buf = (uint8_t*)malloc((size_t)samples);
    if (!buf) { printf("\n  out of memory\n"); return; }

    hal_gpio_set_dir(pin, false);
    hal_gpio_set_pull(pin, true, false);

    for (int i = 0; i < samples; i++) {
        buf[i] = (uint8_t)hal_gpio_get(pin);
        if (i % 32 == 0) printf(".");
        hal_sleep_us((uint32_t)us_per_sample);
    }
    printf(" done.\n\n");

    int edges = 0, highs = 0;
    uint32_t widths[256];
    int wcount = 0;
    int run = 1;
    int prev = buf[0];

    for (int i = 1; i < samples; i++) {
        if (buf[i] == prev) { run++; }
        else {
            edges++;
            if (wcount < 256) widths[wcount++] = (uint32_t)run;
            run = 1; prev = buf[i];
        }
        if (buf[i]) highs++;
    }
    if (run > 0 && wcount < 256) widths[wcount++] = (uint32_t)run;

    float duty = (float)highs / (float)samples * 100.0f;
    float window_ms = (float)(samples * us_per_sample) / 1000.0f;

    uint32_t min_w = 0xFFFFFFFF, max_w = 0;
    for (int i = 0; i < wcount; i++) {
        if (widths[i] < min_w) min_w = widths[i];
        if (widths[i] > max_w) max_w = widths[i];
    }

    uint32_t min_us = min_w * (uint32_t)us_per_sample;
    uint32_t max_us = max_w * (uint32_t)us_per_sample;

    printf("  edges      : %d\n", edges);
    printf("  duty cycle : %.1f%%\n", duty);
    printf("  min pulse  : %lu us\n", min_us);
    printf("  max pulse  : %lu us\n", max_us);
    printf("  window     : %.2f ms\n", window_ms);

    printf("\n  PROTOCOL ANALYSIS:\n");

    if (edges == 0) {
        printf("  -> line is %s (stuck %s)\n",
               buf[0] ? "HIGH" : "LOW",
               buf[0] ? "(idle UART or NC)" : "(stuck low - check GND)");
        free(buf);
        return;
    }

    if (min_us >= 5) {
        uint32_t raw_baud = 1000000 / min_us;
        uint32_t snapped = 0;
        uint32_t best_err = 0xFFFFFFFF;
        for (int i = 0; i < BAUD_COUNT; i++) {
            uint32_t err = (raw_baud > COMMON_BAUDS[i]) ?
                raw_baud - COMMON_BAUDS[i] : COMMON_BAUDS[i] - raw_baud;
            if (err < best_err) { best_err = err; snapped = COMMON_BAUDS[i]; }
        }
        float pct_err = (float)best_err / (float)snapped * 100.0f;
        if (pct_err < 10.0f) {
            printf("  -> likely UART\n");
            printf("     estimated baud : ~%lu (%.1f%% from std %lu)\n",
                   raw_baud, pct_err, snapped);
            printf("     idle state     : %s\n", buf[0] ? "HIGH (normal)" : "LOW (inverted)");
            printf("     tip: uart %lu <tx_pin> %d\n", snapped, pin);
        }
    }

    if (min_us <= 10 && max_us >= 500) {
        printf("  -> possible I2C (mixed pulse widths, bursts)\n");
        printf("     If SCL pin: ~%.0f kHz clock\n",
               min_us > 0 ? 1000.0f / (2.1f * (float)min_us) : 0.0f);
        printf("     tip: i2c scan (SDA=GP4 SCL=GP5)\n");
    }

    if (edges > 20 && duty > 40.0f && duty < 60.0f && min_us < 50) {
        printf("  -> possible SPI clock line\n");
        printf("     tip: spi init, then spi xfer\n");
    }

    if (edges < 10 && min_us > 500) {
        if (min_us >= 500 && max_us <= 2500) {
            printf("  -> likely servo PWM (pulse %lu-%lu us)\n", min_us, max_us);
            printf("     tip: servo <pin> <angle>\n");
        } else {
            printf("  -> likely PWM signal  (~%.0f Hz, %.1f%% duty)\n",
                   edges > 1 ? 1000.0f / window_ms * (edges / 2.1f) : 0.0f, duty);
        }
    }

    if (edges >= 2) {
        float freq = (float)edges / 2.1f / (window_ms / 1000.0f);
        printf("  -> signal frequency : ~%.1f Hz  (from %d edges)\n", freq, edges);
    }

    free(buf);
}
