#include "oled_console.h"
#include "oled.h"
#include <string.h>
#include <stdio.h>
#include "hal.h"

#define CON_ROWS  8
#define CON_COLS  21

static char     s_buf[CON_ROWS][CON_COLS + 1];
static int      s_row = 0;
static int      s_col = 0;
static bool     s_enabled = false;
static bool     s_dirty = false;
static uint64_t s_last_render_ms = 0;

static int s_esc = 0;

static void con_clear(void) {
    for (int r = 0; r < CON_ROWS; r++) s_buf[r][0] = '\0';
    s_row = s_col = 0;
}

static void con_scroll(void) {
    for (int r = 1; r < CON_ROWS; r++)
        memcpy(s_buf[r - 1], s_buf[r], CON_COLS + 1);
    s_buf[CON_ROWS - 1][0] = '\0';
    s_row = CON_ROWS - 1;
    s_col = 0;
}

static void con_newline(void) {
    s_col = 0;
    if (s_row < CON_ROWS - 1) s_row++;
    else con_scroll();
}

static void con_putc(char c) {
    if (s_esc == 1) { s_esc = (c == '[') ? 2 : 0; return; }
    if (s_esc == 2) { if ((c >= '@' && c <= '~')) s_esc = 0; return; }

    switch (c) {
        case 27:   s_esc = 1; return;
        case '\r': s_col = 0; return;
        case '\n': con_newline(); s_dirty = true; return;
        case '\b': if (s_col > 0) { s_col--; s_buf[s_row][s_col] = '\0'; } return;
        case '\t': c = ' '; break;
        default: break;
    }
    if (c < 32 || c > 126) return;

    if (s_col >= CON_COLS) con_newline();
    s_buf[s_row][s_col++] = c;
    s_buf[s_row][s_col]   = '\0';
    s_dirty = true;
}

static void con_render(void) {
    if (!s_dirty) return;
    uint64_t now = hal_time_ms();
    if (now - s_last_render_ms < 50) return;
    s_last_render_ms = now;
    oled_clear();
    for (int r = 0; r < CON_ROWS; r++)
        oled_text(0, r, s_buf[r]);
    oled_display();
    s_dirty = false;
}

void oled_console_write(const char* s) {
    if (!s_enabled) return;
    while (*s) {
        con_putc(*s);
        if (*s == '\n') con_render();
        s++;
    }
}

void oled_console_enable(bool on) {
    if (on == s_enabled) return;
    s_enabled = on;
    if (on) {
        con_clear();
        s_dirty = true;
        oled_clear();
        oled_display();
    }
}

bool oled_console_enabled(void) { return s_enabled; }

void oled_console_poll(void) {
    if (s_enabled) con_render();
}
