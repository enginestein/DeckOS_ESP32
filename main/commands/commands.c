#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "hal.h"
#include "bg_job.h"
#include "file_persist.h"
#include "editor.h"
#include "module.h"
#include "oled.h"
#include "print_lock.h"
#include "commands.h"
#include "kernel.h"
#include "dscript.h"
#include "drivers.h"
#include "vfs.h"
#include "uart_detect.h"
#include "board_detect.h"
#include "scheduler.h"
#include "config.h"
#include "syslog.h"
#include "gpio_mon.h"
#include "morse.h"
#include "tone.h"
#include "servo.h"
#include "nrf24l01.h"
#include "spi_bus.h"
#include "uart_pass.h"
#include "bt.h"
#include "device_detect.h"
#include "bench.h"
#include "heap_track.h"
#include "swarm.h"
#include "esp_heap_caps.h"
#include "shell.h"
#include "commands.h"
#include "kernel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "esp_efuse.h"
#include "esp_mac.h"
#include "esp_system.h"

static void cmd_usb(int argc, char *argv[]) { (void)argc; (void)argv; printf("usb: not implemented\n"); }
static void cmd_hid(int argc, char *argv[]) { (void)argc; (void)argv; printf("hid: not implemented\n"); }
static void cmd_console(int argc, char *argv[]) { (void)argc; (void)argv; printf("console: not implemented\n"); }
extern void cmd_imu(int argc, char *argv[]);
extern void cmd_ota(int argc, char *argv[]);
extern void cmd_dashboard(int argc, char *argv[]);

extern flash_config_t g_config;

static uint32_t s_cmd_count = 0;
static uint32_t s_unknown_count = 0;
static uint64_t s_boot_us;

static void print_uptime(void) {
    uint64_t us = hal_time_us();
    uint32_t s = (uint32_t)(us / 1000000);
    printf("%02luh %02lum %02lus", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60), (unsigned long)(s % 60));
}

static void cmd_help(int argc, char *argv[]) {
    typedef struct { const char *name; const char *desc; } entry_t;
    typedef struct { const char *group; const entry_t *cmds; int count; } group_t;

    static const entry_t g_core[] = {
        {"help", "show this help"},
        {"version", "OS version and build info"},
        {"clear", "clear terminal screen"},
        {"echo", "echo <text>"},
        {"uptime", "time since boot"},
        {"sysinfo", "full system summary"},
        {"stats", "runtime statistics"},
        {"top", "live task monitor"},
    };
    static const entry_t g_hardware[] = {
        {"temp", "internal core temperature"},
        {"mem", "memory overview"},
        {"free", "heap allocator stats"},
        {"power", "VSYS voltage and battery estimate"},
        {"gpio", "read / write / mode / irq <pin>"},
        {"led", "on | off | toggle | blink [n]"},
        {"pwm", "pwm <pin> <duty 0-100> [freq_hz]"},
        {"adc", "raw ADC read (ch 0-2, GPIO26-28)"},
        {"avg", "averaged ADC read [samples]"},
        {"pull", "pull <pin> up | down | none"},
        {"pin", "snapshot of all GPIO states"},
        {"wdog", "watchdog status"},
        {"board", "ESP32 board info"},
        {"psram", "PSRAM size and free"},
        {"flash", "read|write|erase raw SPI flash access"},
    };
    static const entry_t g_buses[] = {
        {"i2c", "scan [sda scl] | read | write | dump"},
        {"spi", "init | write | read | xfer"},
        {"uart", "passthrough <baud> <tx> <rx> [timeout_s]"},
    };
    static const entry_t g_probes[] = {
        {"la", "logic analyser <pin> [samples] [us] [trigger]"},
        {"scope", "<pin> <hz> <ms> clean waveform viewer"},
        {"detect", "scan | uart <pin> | analyze <pin>"},
    };
    static const entry_t g_scripting[] = {
        {"sleep", "sleep <ms>"},
        {"repeat", "repeat <n> <command>"},
        {"watch", "watch <ms> <command> run at interval"},
        {"trigger", "trigger <pin> <rise|fall|both> <cmd>"},
        {"cron", "cron <delay_ms> <command> deferred run"},
        {"bench", "bench <iters> <cmd> throughput test"},
        {"time", "time <cmd> measure execution time"},
        {"alias", "alias [name [cmd...]] define/list aliases"},
        {"unalias", "unalias <name> remove an alias"},
        {"script", "<file> run a DeckOS script file"},
        {"run", "<commands...> run inline script commands"},
    };
    static const entry_t g_system[] = {
        {"reboot", "reboot via watchdog"},
        {"drivers", "list loaded drivers"},
        {"tasks", "list / enable / disable background tasks"},
        {"config", "show | set | save | reset"},
        {"syslog", "show | warn | err | write | clear | stats"},
        {"jobs", "list / cancel background jobs"},
        {"date", "[set Y M D h m s] real-time clock"},
        {"history", "[clear] command history"},
        {"uname", "[-a] system identity"},
        {"rand", "[min] [max] hardware random number"},
        {"edit", "<file> open text editor (needs editor module)"},
        {"module", "load|unload|list <name> manage modules"},
        {"clock", "CPU clock frequency"},
        {"stack", "current task stack high water mark"},
        {"uid", "chip unique identifier (MAC)"},
        {"fault", "panic/fault info"},
    };
    static const entry_t g_fs[] = {
        {"ls", "list directory [path]"},
        {"cat", "print file contents"},
        {"touch", "create / update file"},
        {"mkdir", "create directory"},
        {"rm", "rm [-r] <path> remove file or dir"},
        {"write", "overwrite file with text"},
        {"append", "append text to file"},
        {"hexdump", "hex + ASCII dump"},
        {"cd", "change directory"},
        {"pwd", "print working directory"},
        {"cp", "cp <src> <dst>"},
        {"mv", "mv <src> <dst> move / rename"},
        {"stat", "file / dir metadata"},
        {"wc", "count lines, words, bytes"},
        {"grep", "grep <pattern> <file>"},
        {"find", "recursive name search"},
        {"df", "filesystem usage summary"},
        {"tree", "print directory tree"},
        {"save", "persist VFS to flash"},
    };
    static const entry_t g_usb[] = {
        {"usb", "status|list|export|import|sync|rm|format USB mass-storage disk"},
        {"hid", "type <text>|line <text>|key <COMBO>|enter act as USB keyboard"},
        {"console", "oled on|off mirror shell output to the OLED (handheld mode)"},
    };

#define COUNT(a) (int)(sizeof(a) / sizeof((a)[0]))
    static const group_t groups[] = {
        {"core",       g_core,     COUNT(g_core)},
        {"hardware",   g_hardware, COUNT(g_hardware)},
        {"buses",      g_buses,    COUNT(g_buses)},
        {"probes",     g_probes,   COUNT(g_probes)},
        {"scripting",  g_scripting,COUNT(g_scripting)},
        {"system",     g_system,   COUNT(g_system)},
        {"filesystem", g_fs,       COUNT(g_fs)},
        {"usb",        g_usb,      COUNT(g_usb)},
    };
#undef COUNT

    static const int group_count = (int)(sizeof(groups) / sizeof(group_t));
    static const int NAME_COL = 22;

    if (argc > 1) {
        const char *wanted = argv[1];
        for (int g = 0; g < group_count; g++) {
            if (strcmp(wanted, groups[g].group) == 0) {
                printf("\n[%s]\n", groups[g].group);
                printf("----------------------------------------------------\n");
                fflush(stdout);
                for (int i = 0; i < groups[g].count; i++)
                    printf("  %-*s %s\n", NAME_COL, groups[g].cmds[i].name, groups[g].cmds[i].desc);
                printf("----------------------------------------------------\n");
                fflush(stdout);
                return;
            }
        }
        printf("unknown help group: %s\n", wanted);
        fflush(stdout);
        return;
    }

    printf("DeckOS v3.0  --  command groups\n");
    printf("====================================================\n\n");
    fflush(stdout);
    for (int g = 0; g < group_count; g++)
        printf("  %-20s help %s\n", groups[g].group, groups[g].group);
    printf("\n----------------------------------------------------\n");
    printf("example: help core\n");
    fflush(stdout);
}

static void cmd_version(int argc, char *argv[]) {
    (void)argc; (void)argv;
    const board_info_t *b = board_detect();
    printf("DeckOS v3.0  |  %s\n", b->name);
    printf("Build: %s %s\n", __DATE__, __TIME__);
}

static void cmd_clear(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("\033[2J\033[H");
}

static void cmd_echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++)
        printf("%s%s", argv[i], (i < argc - 1) ? " " : "");
    printf("\n");
}

static void cmd_uptime(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("uptime: ");
    print_uptime();
    printf("\n");
}

static void cmd_temp(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("core temp: N/A (ESP32 temp sensor not exposed via HAL)\n");
    fflush(stdout);
}

static void cmd_mem(int argc, char *argv[]) {
    (void)argc; (void)argv;
    const board_info_t *b = board_detect();
    printf("total flash : %lu KB\n", b->flash_kb);
    printf("PSRAM      : %s\n", b->has_psram ? "yes" : "no");
    printf("SD card    : %s\n", b->has_sdcard ? "yes" : "no");
    printf("camera     : %s\n", b->has_camera ? "yes" : "no");
    fflush(stdout);
}

static void cmd_wdog(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("watchdog: ESP32 auto-reboots on hang\n");
    printf("last reboot cause: check ESP32 RTC reset reason\n");
}

static void cmd_pin(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("GPIO state snapshot:\n");
    printf("PIN  DIR  VAL\n");
    fflush(stdout);
    for (int i = 0; i <= 39; i++) {
        hal_gpio_init(i);
        int val = hal_gpio_get(i);
        printf(" %-3d  %-4s  %d\n", i, "IN", val);
    }
    fflush(stdout);
}


static void argv_join(char *buf, int buflen, int argc, char *argv[], int start) {
    buf[0] = '\0';
    for (int i = start; i < argc; i++) {
        if (i > start) strncat(buf, " ", (size_t)(buflen - 1) - strlen(buf));
        strncat(buf, argv[i], (size_t)(buflen - 1) - strlen(buf));
    }
    int len = (int)strlen(buf);
    if (len >= 2 && buf[0] == '"' && buf[len - 1] == '"') {
        memmove(buf, buf + 1, (size_t)(len - 2));
        buf[len - 2] = '\0';
    }
}

static void cmd_ls(int argc, char *argv[]) {
    vfs_ls(argc >= 2 ? argv[1] : ".");
}

static void cmd_cat(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: cat <file> [file2 ...]\n"); return; }
    for (int a = 1; a < argc; a++) {
        if (argc > 2) printf("==> %s <==\n", argv[a]);
        vfs_cat(argv[a]);
    }
}

static void cmd_touch(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: touch <file> [file2 ...]\n"); return; }
    bool changed = false;
    for (int a = 1; a < argc; a++) {
        if (vfs_touch(argv[a]) >= 0) { printf("touched '%s'\n", argv[a]); changed = true; }
    }
    if (changed) vfs_save();
}

static void cmd_mkdir(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: mkdir <dir> [dir2 ...]\n"); return; }
    bool changed = false;
    for (int a = 1; a < argc; a++) {
        if (vfs_mkdir(argv[a]) >= 0) { printf("created dir '%s'\n", argv[a]); changed = true; }
    }
    if (changed) vfs_save();
}

static void cmd_rm(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: rm [-r] <path> [path2 ...]\n"); return; }
    bool recursive = false;
    int start = 1;
    if (strcmp(argv[1], "-r") == 0) { recursive = true; start = 2; }
    if (start >= argc) { printf("rm: missing path argument\n"); return; }
    bool changed = false;
    for (int a = start; a < argc; a++) {
        if (vfs_rm(argv[a], recursive) == 0) { printf("removed '%s'\n", argv[a]); changed = true; }
    }
    if (changed) vfs_save();
}

static bool interactive_write(const char *path) {
    printf("iwrite: writing to '%s'\n", path);
    printf("  paste or type content; end with '.' on its own line.\n");
    printf("  type '.abort' to cancel.\n");
    printf("---\n");
    bool first_line = true;
    int total = 0;
    while (true) {
        char line[SCRIPT_LINE_LEN];
        int lpos = 0;
        bool line_done = false;
        while (!line_done && lpos < (int)(sizeof(line) - 1)) {
            int c = hal_console_getchar();
            if (c < 0) { kernel_poll(); hal_sleep_ms(1); continue; }
            if (c == '\r' || c == '\n') {
                putchar('\n');
                fflush(stdout);
                line_done = true;
            } else if (c == 3) {
                printf("\niwrite: cancelled\n");
                return false;
            } else if ((c == 127 || c == '\b') && lpos > 0) {
                lpos--;
                printf("\b \b");
                fflush(stdout);
            } else if (c >= 32 && c < 127) {
                line[lpos++] = (char)c;
                putchar(c);
                fflush(stdout);
            }
        }
        line[lpos] = '\0';
        if (strcmp(line, ".") == 0) break;
        if (strcmp(line, ".abort") == 0) {
            printf("iwrite: aborted -- nothing written\n");
            return false;
        }
        uint8_t wbuf[SCRIPT_LINE_LEN + 1];
        memcpy(wbuf, line, (size_t)lpos);
        wbuf[lpos] = '\n';
        int n = vfs_write(path, wbuf, (uint32_t)(lpos + 1), !first_line);
        if (n < 0) { printf("iwrite: write error on '%s'\n", path); return false; }
        total += lpos + 1;
        first_line = false;
    }
    if (total == 0) { printf("iwrite: empty content -- nothing written\n"); return false; }
    printf("---\niwrite: saved %d bytes to '%s'\n", total, path);
    return true;
}

static void cmd_write(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  write <file> <content...>    overwrite file with one line of text\n");
        printf("  write -i <file>              interactive multi-line write (end with '.')\n");
        return;
    }
    if (strcmp(argv[1], "-i") == 0) {
        if (argc < 3) { printf("write -i: missing filename\n"); return; }
        if (interactive_write(argv[2])) vfs_save();
        return;
    }
    if (argc < 3) {
        printf("usage: write <file> <content...>\n");
        printf("       use 'write -i <file>' for multi-line interactive input\n");
        return;
    }
    char content[VFS_MAX_FILE_SIZE];
    argv_join(content, sizeof(content), argc, argv, 2);
    int clen = (int)strlen(content);
    if (clen < (int)sizeof(content) - 1) { content[clen] = '\n'; content[clen + 1] = '\0'; }
    int n = vfs_write(argv[1], (const uint8_t *)content, (uint32_t)strlen(content), false);
    if (n >= 0) { printf("wrote %d B to '%s'\n", n, argv[1]); vfs_save(); }
}

static void cmd_iwrite(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: iwrite <file>\n");
        printf("  Enter or paste lines, then type '.' alone to save.\n");
        printf("  Type '.abort' to cancel.\n");
        return;
    }
    if (interactive_write(argv[1])) vfs_save();
}

static void cmd_append(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: append <file> <content...>\n"); return; }
    char content[VFS_MAX_FILE_SIZE];
    argv_join(content, sizeof(content), argc, argv, 2);
    int clen = (int)strlen(content);
    if (clen < VFS_MAX_FILE_SIZE - 1) { content[clen] = '\n'; content[clen + 1] = '\0'; }
    int n = vfs_write(argv[1], (const uint8_t *)content, (uint32_t)strlen(content), true);
    if (n >= 0) { printf("appended %d B to '%s'\n", n, argv[1]); vfs_save(); }
}

static void cmd_hexdump(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: hexdump <file>\n"); return; }
    vfs_hexdump(argv[1]);
}

static void cmd_cd(int argc, char *argv[]) {
    const char *path = (argc >= 2) ? argv[1] : "/";
    if (vfs_cd(path)) printf("%s\n", vfs_cwd_path());
}

static void cmd_pwd(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vfs_pwd();
}

static void cmd_cp(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: cp <src> <dst>\n"); return; }
    if (vfs_cp(argv[1], argv[2]) == 0) vfs_save();
}

static void cmd_mv(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: mv <src> <dst>\n"); return; }
    if (vfs_mv(argv[1], argv[2]) == 0) vfs_save();
}

static void cmd_stat(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: stat <path>\n"); return; }
    for (int a = 1; a < argc; a++) {
        if (argc > 2) printf("==> %s\n", argv[a]);
        vfs_stat(argv[a]);
    }
}

static void cmd_wc(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: wc <file> [file2 ...]\n"); return; }
    for (int a = 1; a < argc; a++) vfs_wc(argv[a]);
}

static void cmd_grep(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: grep <pattern> <file>\n"); return; }
    vfs_grep(argv[2], argv[1]);
}

static void cmd_find(int argc, char *argv[]) {
    const char *name = (argc >= 2) ? argv[1] : "";
    vfs_find_all(name);
}

static void cmd_df(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("=== VFS disk usage ===\n");
    vfs_df();
    printf("======================\n");
}

static void cmd_tree(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vfs_tree();
}

static void cmd_pwm(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: pwm <pin> <duty 0-100> [freq_hz]\n"); return; }
    int pin = atoi(argv[1]);
    int duty = atoi(argv[2]);
    int freq = (argc >= 4) ? atoi(argv[3]) : 1000;
    if (duty < 0 || duty > 100) { printf("duty must be 0-100\n"); return; }
    if (freq < 1 || freq > 40000000) { printf("freq must be 1-40000000 Hz\n"); return; }
    hal_pwm_init(pin);
    hal_pwm_set_duty(pin, (float)duty, (uint32_t)freq);
    printf("PWM on GPIO%-2d  duty=%d%%  freq=%d Hz\n", pin, duty, freq);
}

static void cmd_pull(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: pull <pin> <up|down|none>\n"); return; }
    int pin = atoi(argv[1]);
    hal_gpio_init(pin);
    if (strcmp(argv[2], "up") == 0) {
        hal_gpio_set_pull(pin, true, false);
        printf("GPIO%d pull-up\n", pin);
    } else if (strcmp(argv[2], "down") == 0) {
        hal_gpio_set_pull(pin, false, true);
        printf("GPIO%d pull-down\n", pin);
    } else if (strcmp(argv[2], "none") == 0) {
        hal_gpio_set_pull(pin, false, false);
        printf("GPIO%d pulls disabled\n", pin);
    } else printf("unknown: %s\n", argv[2]);
}

static void cmd_led(int argc, char *argv[]) {
    const board_info_t *b = board_detect();
    int led_pin = b->led_pin;
    static bool led_state = false;
    hal_gpio_init(led_pin);
    hal_gpio_set_dir(led_pin, true);
    if (argc < 2) { printf("usage: led <on|off|toggle|blink [n]>\n"); return; }
    if (strcmp(argv[1], "on") == 0) {
        led_state = true;
        hal_gpio_put(led_pin, 1);
        printf("LED on\n");
    } else if (strcmp(argv[1], "off") == 0) {
        led_state = false;
        hal_gpio_put(led_pin, 0);
        printf("LED off\n");
    } else if (strcmp(argv[1], "toggle") == 0) {
        led_state = !led_state;
        hal_gpio_put(led_pin, led_state);
        printf("LED %s\n", led_state ? "on" : "off");
    } else if (strcmp(argv[1], "blink") == 0) {
        int n = (argc >= 3) ? atoi(argv[2]) : 5;
        if (n < 1 || n > 50) { printf("count 1-50\n"); return; }
        for (int i = 0; i < n; i++) {
            hal_gpio_put(led_pin, 1);
            hal_sleep_ms(150);
            hal_gpio_put(led_pin, 0);
            hal_sleep_ms(150);
        }
        printf("blinked %d times\n", n);
    } else printf("unknown led subcommand: %s\n", argv[1]);
}

static void cmd_gpio(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: gpio <read|write|mode|irq> <pin> [val]\n"); return; }

    if (strcmp(argv[1], "irq") == 0) {
        int pin = atoi(argv[2]);
        if (argc >= 4 && strcmp(argv[3], "stop") == 0) {
            gpio_mon_stop((uint8_t)pin);
            printf("stopped IRQ monitor on GPIO%d\n", pin);
        } else if (argc >= 4 && strcmp(argv[3], "dump") == 0) {
            gpio_mon_dump((uint8_t)pin);
        } else {
            uint32_t timeout_s = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 30;
            gpio_mon_start((uint8_t)pin, timeout_s);
        }
        return;
    }

    int pin = atoi(argv[2]);

    if (strcmp(argv[1], "read") == 0) {
        hal_gpio_init(pin);
        hal_gpio_set_dir(pin, false);
        printf("GPIO%-2d = %d\n", pin, hal_gpio_get(pin));
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc < 4) { printf("usage: gpio write <pin> <0|1>\n"); return; }
        int val = atoi(argv[3]);
        hal_gpio_init(pin);
        hal_gpio_set_dir(pin, true);
        hal_gpio_put(pin, val ? 1 : 0);
        printf("GPIO%-2d <- %d\n", pin, val ? 1 : 0);
    } else if (strcmp(argv[1], "mode") == 0) {
        if (argc < 4) { printf("usage: gpio mode <pin> <in|out>\n"); return; }
        hal_gpio_init(pin);
        if (strcmp(argv[3], "in") == 0) {
            hal_gpio_set_dir(pin, false);
            printf("GPIO%-2d -> INPUT\n", pin);
        } else if (strcmp(argv[3], "out") == 0) {
            hal_gpio_set_dir(pin, true);
            printf("GPIO%-2d -> OUTPUT\n", pin);
        } else printf("unknown mode: %s\n", argv[3]);
    } else {
        printf("unknown gpio subcommand: %s\n", argv[1]);
    }
}

static void cmd_adc(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: adc <0|1|2>  (GPIO26-28)\n"); fflush(stdout); return; }
    int ch = atoi(argv[1]);
    if (ch < 0 || ch > 2) { printf("channel must be 0-2\n"); fflush(stdout); return; }
    hal_adc_init();
    hal_adc_select_input((uint)ch);
    uint16_t raw = hal_adc_read();
    float voltage = raw * 3.3f / 4095.0f;
    printf("ADC%d  raw=%4d  voltage=%.3f V\n", ch, raw, voltage);
    fflush(stdout);
}

static void cmd_avg(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: avg <adc_ch 0-2> [samples 1-256]\n"); fflush(stdout); return; }
    int ch = atoi(argv[1]);
    if (ch < 0 || ch > 2) { printf("channel must be 0-2\n"); fflush(stdout); return; }
    int samples = (argc >= 3) ? atoi(argv[2]) : 64;
    if (samples < 1 || samples > 256) { printf("samples: 1-256\n"); fflush(stdout); return; }

    hal_adc_init();
    hal_adc_select_input((uint)ch);
    uint32_t sum = 0;
    uint16_t mn = 0xFFFF, mx = 0;
    for (int i = 0; i < samples; i++) {
        uint16_t r = hal_adc_read();
        sum += r;
        if (r < mn) mn = r;
        if (r > mx) mx = r;
    }
    uint16_t avg_raw = (uint16_t)(sum / samples);
    float voltage = avg_raw * 3.3f / 4095.0f;
    printf("ADC%d  samples=%d\n", ch, samples);
    printf("  avg  : %4d  /  %.3f V\n", avg_raw, voltage);
    printf("  min  : %4d  /  %.3f V\n", mn, mn * 3.3f / 4095.0f);
    printf("  max  : %4d  /  %.3f V\n", mx, mx * 3.3f / 4095.0f);
    printf("  noise: %4d  counts p-p\n", mx - mn);
    fflush(stdout);
}

static void cmd_i2c(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  i2c scan [sda] [scl]              - scan bus (default GP21 GP22)\n");
        printf("  i2c read  <addr> <reg> [sda] [scl]\n");
        printf("  i2c write <addr> <reg> <val> [sda] [scl]\n");
        printf("  i2c dump  <addr> [sda] [scl]      - dump 256 registers\n");
        return;
    }

    uint sda, scl;
    if (argc >= 4 && isdigit((unsigned char)argv[argc-2][0]) && isdigit((unsigned char)argv[argc-1][0])) {
        scl = (uint)atoi(argv[argc-1]);
        sda = (uint)atoi(argv[argc-2]);
    } else {
        const board_info_t *b = board_detect();
        sda = b->default_i2c_sda;
        scl = b->default_i2c_scl;
    }

    if (strcmp(argv[1], "scan") == 0) {
        hal_i2c_init(sda, scl, 100000);
        printf("I2C scan (SDA=GP%u SCL=GP%u):\n", sda, scl);
        printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
        int found = 0;
        for (int row = 0; row < 8; row++) {
            char line[64];
            int pos = 0;
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X: ", row * 16);
            for (int col = 0; col < 16; col++) {
                uint8_t addr = (uint8_t)(row * 16 + col);
                uint8_t rxdata;
                if (hal_i2c_read(addr, &rxdata, 1) >= 0) {
                    pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", addr);
                    found++;
                } else {
                    pos += snprintf(line + pos, sizeof(line) - pos, "-- ");
                }
            }
            print_lock_acquire();
            printf("%s\n", line);
            print_lock_release();
        }
        print_lock_acquire();
        printf("%d device(s) found\n", found);
        print_lock_release();
    } else if (strcmp(argv[1], "read") == 0) {
        if (argc < 4) { printf("usage: i2c read <addr_hex> <reg_hex> [sda] [scl]\n"); return; }
        uint8_t addr = (uint8_t)strtol(argv[2], NULL, 16);
        uint8_t reg = (uint8_t)strtol(argv[3], NULL, 16);
        hal_i2c_init(sda, scl, 100000);
        uint8_t val = 0;
                int ret = hal_i2c_write_read(addr, (const uint8_t*)&reg, 1, &val, 1);
        if (ret < 0) { printf("I2C error (no ACK?)\n"); return; }
        printf("0x%02X reg[0x%02X] = 0x%02X (%d)\n", addr, reg, val, val);
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc < 5) { printf("usage: i2c write <addr_hex> <reg_hex> <val_hex> [sda] [scl]\n"); return; }
        uint8_t addr = (uint8_t)strtol(argv[2], NULL, 16);
        uint8_t reg = (uint8_t)strtol(argv[3], NULL, 16);
        uint8_t val = (uint8_t)strtol(argv[4], NULL, 16);
        hal_i2c_init(sda, scl, 100000);
        uint8_t buf[2] = {reg, val};
        int ret = hal_i2c_write(addr, buf, 2);
        if (ret < 0) { printf("I2C write failed\n"); return; }
        printf("wrote 0x%02X -> 0x%02X[0x%02X]\n", val, addr, reg);
    } else if (strcmp(argv[1], "dump") == 0) {
        if (argc < 3) { printf("usage: i2c dump <addr_hex> [sda] [scl]\n"); return; }
        uint8_t addr = (uint8_t)strtol(argv[2], NULL, 16);
        hal_i2c_init(sda, scl, 100000);
        printf("Dumping I2C device 0x%02X\n", addr);
        printf("     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
        for (int row = 0; row < 16; row++) {
            printf("%02X: ", row * 16);
            for (int col = 0; col < 16; col++) {
                uint8_t reg = (uint8_t)(row * 16 + col);
                uint8_t val = 0;
        int ret = hal_i2c_write_read(addr, (const uint8_t*)&reg, 1, &val, 1);
                if (ret < 0) printf("-- ");
                else printf("%02X ", val);
            }
            printf("\n");
        }
    } else {
        printf("unknown i2c subcommand: %s\n", argv[1]);
    }
}

static void cmd_oled(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  oled init [sda] [scl]          init display\n");
        printf("  oled on                        turn on display\n");
        printf("  oled off                       turn off display\n");
        printf("  oled clear                     clear screen\n");
        printf("  oled text <x> <y> \"text\"       draw text\n");
        printf("  oled line <x0> <y0> <x1> <y1>  draw line\n");
        printf("  oled rect <x> <y> <w> <h>      draw rect outline\n");
        printf("  oled fill <x> <y> <w> <h>      filled rect\n");
        printf("  oled pixel <x> <y> <0|1>       set pixel\n");
        printf("  oled frame <x> <y> <w> <h>     thick border\n");
        printf("  oled inv                       invert display\n");
        printf("  oled scroll <h|v|hv|stop>\n");
        printf("  oled char <c> [0-3 scale]      draw large char\n");
        printf("  oled image <x> <y>             128x64 raw image\n");
        printf("  oled logo                      DeckOS logo\n");
        printf("  oled demo                      various demos\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        uint sda = (argc >= 3) ? (uint)atoi(argv[2]) : 21;
        uint scl = (argc >= 4) ? (uint)atoi(argv[3]) : 22;
        oled_init(sda, scl);
        printf("OLED init SDA=GP%u SCL=GP%u\n", sda, scl);
    } else if (strcmp(argv[1], "on") == 0) {
        oled_on();
    } else if (strcmp(argv[1], "off") == 0) {
        oled_off();
    } else if (strcmp(argv[1], "clear") == 0) {
        oled_clear();
    } else if (strcmp(argv[1], "text") == 0 || strcmp(argv[1], "string") == 0) {
        if (argc < 5) { printf("usage: oled text <x> <y> \"text\"\n"); return; }
        int x = atoi(argv[2]), y = atoi(argv[3]);
        oled_text(x, y, argv[4]);
    } else if (strcmp(argv[1], "pixel") == 0) {
        if (argc < 5) { printf("usage: oled pixel <x> <y> <0|1>\n"); return; }
        oled_pixel(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]) ? 1 : 0);
    } else if (strcmp(argv[1], "inv") == 0) {
        oled_invert(true);
    } else if (strcmp(argv[1], "display") == 0) {
        oled_display();
    } else { printf("unavailable oled subcommand: %s\n", argv[1]); return; }
}

static void cmd_spi(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  spi init <miso> <mosi> <sck> <baud> <mode>  - init bus\n");
        printf("  spi write <cs> <hex bytes...>               - write bytes\n");
        printf("  spi read  <cs> <count>                      - read bytes\n");
        printf("  spi xfer  <cs> <hex bytes...>               - full duplex\n");
        return;
    }

    if (argc >= 3 && strcmp(argv[1], "init") == 0) {
        if (argc < 7) { printf("usage: spi init <miso> <mosi> <sck> <baud> <mode>\n"); return; }
        int miso = atoi(argv[2]), mosi = atoi(argv[3]), sck = atoi(argv[4]);
        int baud = atoi(argv[5]), mode = atoi(argv[6]);
        hal_spi_t spibus = hal_spi_init((uint)sck, (uint)mosi, (uint)miso, (uint)baud);
        if (spibus == NULL) { printf("SPI init failed\n"); return; }
        printf("SPI initted: MISO=GP%d MOSI=GP%d SCK=GP%d baud=%d mode=%d\n", miso, mosi, sck, baud, mode);
    } else if (argc >= 3 && strcmp(argv[1], "write") == 0) {
        int cs = atoi(argv[2]);
        uint8_t data[128]; int dlen = 0;
        for (int a = 3; a < argc && dlen < 128; a++) {
            unsigned long v = strtoul(argv[a], NULL, 16);
            if (v > 0xFF) { printf("byte value 0-255\n"); return; }
            data[dlen++] = (uint8_t)v;
        }
        hal_spi_init(0, 0, 0, 0);
        spi_bus_transfer(cs, data, NULL, dlen);
        printf("SPI wrote %d bytes to CS=%d\n", dlen, cs);
    } else if (argc >= 3 && strcmp(argv[1], "read") == 0) {
        int cs = atoi(argv[2]);
        int count = (argc >= 4) ? atoi(argv[3]) : 8;
        if (count > 256) count = 256;
        uint8_t tx[256]; memset(tx, 0xFF, count);
        uint8_t rx[256];
        hal_spi_init(0, 0, 0, 0);
        spi_bus_transfer(cs, tx, rx, count);
        printf("SPI read %d bytes from CS=%d:\n", count, cs);
        for (int i = 0; i < count; i++) {
            printf("%02X ", rx[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        if (count % 16) printf("\n");
    } else if (argc >= 3 && strcmp(argv[1], "xfer") == 0) {
        int cs = atoi(argv[2]);
        uint8_t tx[128]; int dlen = 0;
        for (int a = 3; a < argc && dlen < 128; a++) {
            unsigned long v = strtoul(argv[a], NULL, 16);
            if (v > 0xFF) { printf("byte value 0-255\n"); return; }
            tx[dlen++] = (uint8_t)v;
        }
        uint8_t rx[128];
        hal_spi_init(0, 0, 0, 0);
        spi_bus_transfer(cs, tx, rx, dlen);
        printf("SPI xfer %d bytes:\n", dlen);
        for (int i = 0; i < dlen; i++) printf("%02X ", rx[i]);
        printf("\n");
    } else printf("unknown spi subcommand: %s\n", argv[1]);
}

static void cmd_servo(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  servo <pin> <angle_deg>         -- set servo angle\n");
        printf("  servo sweep <pin> [from] [to] [ms] -- sweep 0-180\n");
        printf("  servo bg add <pin> [name]        -- register bg servo\n");
        printf("  servo bg sweep <slot> [min] [max] [step] [ms]\n");
        printf("  servo bg goto <slot> <angle> [ms]\n");
        printf("  servo bg stop <slot>\n");
        printf("  servo bg list\n");
        return;
    }

    if (strcmp(argv[1], "sweep") == 0) {
        int pin = (argc >= 3) ? atoi(argv[2]) : -1;
        if (pin < 0) { printf("usage: servo sweep <pin> [from] [to] [step_ms]\n"); return; }
        int from = (argc >= 4) ? atoi(argv[3]) : 0;
        int to   = (argc >= 5) ? atoi(argv[4]) : 180;
        int ms   = (argc >= 6) ? atoi(argv[5]) : 20;
        servo_sweep_blocking((uint8_t)pin, from, to, ms);
    } else if (argc >= 2 && strcmp(argv[1], "bg") == 0) {
        if (argc < 3) { printf("usage: servo bg <add|sweep|goto|stop|list>\n"); return; }
        if (strcmp(argv[2], "add") == 0) {
            int pin = (argc >= 4) ? atoi(argv[3]) : -1;
            if (pin < 0) { printf("need pin\n"); return; }
            const char *name = (argc >= 5) ? argv[4] : NULL;
            int slot = servo_bg_add((uint8_t)pin, name);
            if (slot >= 0) printf("servo bg added: slot %d, GP%d\n", slot, pin);
        } else if (strcmp(argv[2], "sweep") == 0) {
            if (argc < 4) { printf("usage: servo bg sweep <slot> [min] [max] [step_deg] [step_ms]\n"); return; }
            int slot = atoi(argv[3]);
            int minv = (argc >= 5) ? atoi(argv[4]) : 0;
            int maxv = (argc >= 6) ? atoi(argv[5]) : 180;
            int step = (argc >= 7) ? atoi(argv[6]) : 1;
            int ms   = (argc >= 8) ? atoi(argv[7]) : 20;
            servo_bg_set_sweep(slot, minv, maxv, step, (uint32_t)ms);
            printf("servo slot %d sweep set %d-%d step %d every %d ms\n", slot, minv, maxv, step, ms);
        } else if (strcmp(argv[2], "goto") == 0) {
            if (argc < 5) { printf("usage: servo bg goto <slot> <angle> [step_ms]\n"); return; }
            int slot = atoi(argv[3]), angle = atoi(argv[4]);
            int ms   = (argc >= 6) ? atoi(argv[5]) : 20;
            servo_bg_set_goto(slot, angle, (uint32_t)ms);
            printf("servo slot %d goto %d deg\n", slot, angle);
        } else if (strcmp(argv[2], "stop") == 0) {
            if (argc < 4) { printf("usage: servo bg stop <slot>\n"); return; }
            int slot = atoi(argv[3]);
            servo_bg_stop(slot);
            printf("servo slot %d stopped\n", slot);
        } else if (strcmp(argv[2], "list") == 0) {
            servo_bg_list();
        } else printf("unknown bg subcommand: %s\n", argv[2]);
    } else {
        int pin = atoi(argv[1]);
        int angle = (argc >= 3) ? atoi(argv[2]) : 90;
        int ret = servo_set(pin, angle);
        if (ret >= 0) printf("Servo GP%d set to %d deg\n", pin, angle);
        else printf("Servo error on GP%d\n", pin);
    }
}

static void cmd_tone(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: tone <pin> <note|Hz> [duration_ms]\n");
        printf("Notes: C4 D4 E4 F4 G4 A4 B4 C5 ... (or Hz like 440)\n");
        return;
    }
    int pin = atoi(argv[1]);
    const char *note_or_hz = argv[2];
    int ms = (argc >= 4) ? atoi(argv[3]) : 500;
    if (ms > 30000) ms = 30000;

    if (isdigit((unsigned char)note_or_hz[0])) {
        int hz = atoi(note_or_hz);
        if (hz < 20 || hz > 20000) { printf("Hz out of range (20-20000)\n"); return; }
        tone_play_hz(pin, (uint)hz, (uint)ms);
        printf("Tone %d Hz on GP%d for %d ms\n", hz, pin, ms);
    } else {
        tone_play(pin, note_or_hz, (uint)ms);
        printf("Note %s on GP%d for %d ms\n", note_or_hz, pin, ms);
    }
}

static const char *twinkle[] = {"C4:200","C4:200","G4:200","G4:200","A4:200","A4:200","G4:400",
    "F4:200","F4:200","E4:200","E4:200","D4:200","D4:200","C4:400",NULL};
static const char *elise[] = {"E5:150","Eb5:150","E5:150","Eb5:150","E5:150","B4:150","D5:150",
    "C5:150","A4:600",NULL};
static const char *canon_str[] = {"C4:300","G3:300","A3:300","E3:300","F3:300","C3:300",
    "F3:300","G3:300",NULL};

static void cmd_melody(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: melody <pin> <C4:200 E4:200 ...> | elise | canon\n");
        return;
    }
    int pin = atoi(argv[1]);
    const char **seq = NULL;

    if (argc == 3) {
        if (strcmp(argv[2], "elise") == 0) seq = elise;
        else if (strcmp(argv[2], "canon") == 0) seq = canon_str;
        else if (strcmp(argv[2], "twinkle") == 0) seq = twinkle;
    }

    if (seq) {
        int ticks = 0;
        for (int i = 0; seq[i]; i++) {
            char buf[32];
            strncpy(buf, seq[i], sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            char *colon = strchr(buf, ':');
            if (!colon) continue;
            *colon++ = '\0';
            int ms = atoi(colon);
            tone_play(pin, buf, (uint)ms);
            hal_sleep_ms((uint)ms + 30);
            ticks += ms + 30;
        }
        printf("Melody complete (%d ms)\n", ticks);
        return;
    }

    for (int a = 2; a < argc; a++) {
        char buf[32];
        strncpy(buf, argv[a], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *colon = strchr(buf, ':');
        if (!colon) { printf("bad note format: %s (expect C4:200)\n", argv[a]); continue; }
        *colon++ = '\0';
        int ms = atoi(colon);
        if (ms < 10) ms = 200;
        tone_play(pin, buf, (uint)ms);
        hal_sleep_ms((uint)ms + 20);
    }
}

static void cmd_morse(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: morse <text> [wpm]\n");
        return;
    }
    int wpm = (argc >= 3) ? atoi(argv[2]) : 8;
    if (wpm < 3 || wpm > 40) { printf("wpm: 3-40\n"); return; }
    const board_info_t *b = board_detect();
    int pin = b->led_pin;
    (void)pin;
    morse_send(argv[1], wpm, pin);
    printf("Morse: %s (%d wpm)\n", argv[1], wpm);
}

static void cmd_piano(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: piano <pin>\n"); return; }
    int pin = atoi(argv[1]);
    printf("Piano mode on GP%d\n", pin);
    printf("Press keys on keyboard to play notes.\n");
    printf("Lower row: C D E F G A B (C4-B4)\n");
    printf("Upper row: W E   T Y U (sharps)\n");
    printf("Press Q to quit.\n");

    static const char *notes[] = {"C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4"};
    static const char keymap[] = {
        'a','w','s','e','d','f','t','g','y','h','u','j',0};
    int prev_note = -1;

    while (true) {
        kernel_poll();
        int c = hal_console_getchar();
        if (c < 0) { hal_sleep_ms(10); continue; }
        if (c == 'q' || c == 'Q') break;
        for (int i = 0; keymap[i]; i++) {
            if (c == keymap[i]) {
                if (prev_note == i) break;
                prev_note = i;
                tone_play(pin, notes[i], 500);
                break;
            }
        }
        if (c >= 'a' && c <= 'z') {
            bool found = false;
            for (int i = 0; keymap[i]; i++) if (c == keymap[i]) { found = true; break; }
                if (!found) { tone_stop(); prev_note = -1; }
        } else { tone_stop(); prev_note = -1; }
    }
    tone_stop();
}

static void cmd_la(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: la <pin> [samples] [us] [trigger]\n");
        printf("  Quick logic analyser on any GPIO\n");
        printf("  samples: 64-4096 (default 256)\n");
        printf("  us     : sampling period in us (1-1000, default 10)\n");
        printf("  trigger: 0=off 1=rising 2=falling\n");
        return;
    }
    uint pin = (uint)atoi(argv[1]);
    uint samples = (argc >= 3) ? (uint)atoi(argv[2]) : 256;
    uint period_us = (argc >= 4) ? (uint)atoi(argv[3]) : 10;
    uint trigger = (argc >= 5) ? (uint)atoi(argv[4]) : 1;
    if (samples < 64) samples = 64;
    if (samples > 4096) samples = 4096;
    if (period_us < 1) period_us = 1;
    if (period_us > 1000) period_us = 1000;

    hal_gpio_init(pin);
    hal_gpio_set_dir(pin, false);

    printf("Logic analyser GP%d\n", pin);
    printf("samples: %u, period: %u us, trigger: %s\n",
        samples, period_us, trigger == 0 ? "off" : (trigger == 1 ? "rising" : "falling"));
    printf("Press Ctrl-C to abort\n");

    uint8_t *buf = (uint8_t *)malloc(samples);
    if (!buf) { printf("out of memory\n"); return; }

    if (trigger > 0) {
        int expected = (trigger == 1) ? 0 : 1;
        int want = (trigger == 1) ? 1 : 0;
        printf("Waiting for %s edge...\n", trigger == 1 ? "rising" : "falling");
        while (true) {
            int val = hal_gpio_get(pin);
            if (val == expected) {
                while (hal_gpio_get(pin) != want) hal_sleep_us(1);
                break;
            }
        }
    }

    for (uint i = 0; i < samples; i++) {
        buf[i] = (uint8_t)hal_gpio_get(pin);
        hal_sleep_us(period_us);
    }

    uint32_t ns = period_us * 1000;
    printf("\nLA snapshot (%u samples x %u us = %u ms):\n", samples, period_us, (samples * period_us) / 1000);
    printf("time      signal\n");
    printf("--------  ------\n");
    int run_start = -1;
    int run_val = -1;
    int printed = 0;
    for (uint i = 0; i < samples; i++) {
        if (run_start < 0) {
            run_start = (int)i;
            run_val = buf[i];
        }
        if (i == samples - 1 || buf[i + 1] != run_val) {
            if (run_val == 1 && (i - run_start) > 0) {
                uint32_t t_ns = run_start * ns;
                uint32_t dur = (i - run_start + 1) * period_us;
                printf("%u.%03u ms  HIGH for %lu us  [%d-%d]\n",
                    (unsigned)(t_ns / 1000000), (unsigned)((t_ns % 1000000) / 1000),
                    (unsigned long)dur, run_start, i);
                printed++;
            }
            run_start = -1;
        }
    }
    printf("--- %d high pulses captured ---\n", printed);
    printf("approx freq: %.1f Hz\n", 1000000.0 / (period_us * 2));
    fflush(stdout);
    free(buf);
}

static void cmd_scope(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: scope <pin> <hz> <ms>\n");
        printf("  Clean waveform viewer for regular signals\n");
        printf("  pin: GPIO to sample\n");
        printf("  hz: expected signal frequency\n");
        printf("  ms: capture duration\n");
        return;
    }
    int pin = atoi(argv[1]);
    int hz = atoi(argv[2]);
    int ms = (argc >= 4) ? atoi(argv[3]) : 20;
    if (hz < 1 || ms < 1) { printf("invalid params\n"); return; }

    hal_gpio_init(pin);
    hal_gpio_set_dir(pin, false);

    uint period_us = 1000000 / hz / 20;
    if (period_us < 1) period_us = 1;
    if (period_us > 10000) period_us = 10000;
    int total_samples = (ms * 1000) / period_us;
    if (total_samples > 2048) total_samples = 2048;
    if (total_samples < 10) { printf("capture too short\n"); return; }

    uint8_t *sample = (uint8_t *)malloc(total_samples);
    if (!sample) { printf("OOM\n"); return; }

    printf("Scope GP%d: %d Hz, %d ms (%d samples, %u us period)\n",
        pin, hz, ms, total_samples, period_us);

    for (int i = 0; i < total_samples; i++) {
        sample[i] = (uint8_t)hal_gpio_get(pin);
        hal_sleep_us(period_us);
    }

    int rows = 12, cols = 80;
    for (int r = 0; r < rows; r++) {
        float thresh = 1.0f - (float)r / (rows - 1);
        int pos = 0;
        for (int c = 0; c < cols; c++) {
            int idx = (c * total_samples) / cols;
            if (idx >= total_samples) idx = total_samples - 1;
            int group_size = 1;
            int high_count = 0;
            for (int j = 0; j < group_size && idx + j < total_samples; j++)
                if (sample[idx + j]) high_count++;
            float avg = (float)high_count / group_size;
            pos += (avg > thresh) ? 1 : 0;
            putchar(avg > thresh ? '#' : ' ');
        }
        printf(" %d%%\n", (int)(thresh * 100));
    }
    fflush(stdout);
    free(sample);
}

static void cmd_detect(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  detect scan              - scan for UART & i2c devices\n");
        printf("  detect uart <pin>        - probe one pin for UART activity\n");
        printf("  detect analyze <pin>     - pulse width analysis\n");
        return;
    }

    if (strcmp(argv[1], "scan") == 0) {
        printf("=== Device Detection Scan ===\n");
        device_detect_print(21, 22);
    } else if (strcmp(argv[1], "uart") == 0) {
        if (argc < 3) { printf("need pin\n"); return; }
        uart_detect_run((uint8_t)atoi(argv[2]), 3000);
    } else if (strcmp(argv[1], "analyze") == 0) {
        if (argc < 3) { printf("need pin\n"); return; }
        la_detect_protocol((uint8_t)atoi(argv[2]), 200, 1);
    } else printf("unknown detect: %s\n", argv[1]);
}

static void cmd_sleep(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: sleep <ms>\n"); return; }
    int ms = atoi(argv[1]);
    if (ms < 1) { printf("ms must be > 0\n"); return; }
    if (ms > 10000) { printf("max 10s\n"); return; }
    hal_sleep_ms((uint)ms);
    printf("slept %d ms\n", ms);
}

static void cmd_repeat(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: repeat <n> <command...>\n"); return; }
    int n = atoi(argv[1]);
    if (n < 1 || n > 1000) { printf("n must be 1-1000\n"); return; }
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 2);
    for (int i = 0; i < n; i++) {
        printf("[%d/%d] %s\n", i + 1, n, cmd);
        commands_execute(cmd);
    }
}

static void cmd_watch(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: watch <ms> <command...>\n"); return; }
    int ms = atoi(argv[1]);
    if (ms < 20) ms = 20;
    if (ms > 60000) ms = 60000;
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 2);
    printf("Watching every %d ms: %s\n", ms, cmd);
    int n = 0;
    while (true) {
        printf("[%d]\n", ++n);
        commands_execute(cmd);
        int waited = 0;
        while (waited < ms) {
            kernel_poll();
            hal_sleep_ms(10);
            waited += 10;
            int c = hal_console_getchar();
            if (c >= 0) { printf("-- abort --\n"); return; }
        }
    }
}

static void cmd_trigger(int argc, char *argv[]) {
    if (argc < 4) {
        printf("usage: trigger <pin> <rise|fall|both> <command...>\n");
        return;
    }
    int pin = atoi(argv[1]);
    int edge = 0;
    if (strcmp(argv[2], "rise") == 0) edge = 1;
    else if (strcmp(argv[2], "fall") == 0) edge = 2;
    else if (strcmp(argv[2], "both") == 0) edge = 3;
    else { printf("edge must be: rise fall both\n"); return; }
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 3);

    hal_gpio_init(pin);
    hal_gpio_set_dir(pin, false);
    int last = hal_gpio_get(pin);
    printf("Trigger on GP%d (%s): %s\n", pin, argv[2], cmd);
    printf("Press Ctrl-C to stop\n");

    while (true) {
        int cur = hal_gpio_get(pin);
        if ((edge & 1) && last == 0 && cur == 1) {
            printf("RISE on GP%d -> ", pin);
            commands_execute(cmd);
        }
        if ((edge & 2) && last == 1 && cur == 0) {
            printf("FALL on GP%d -> ", pin);
            commands_execute(cmd);
        }
        last = cur;
        kernel_poll();
        hal_sleep_ms(10);
        int c = hal_console_getchar();
        if (c >= 0) { printf("trigger stopped\n"); return; }
    }
}

static void cmd_cron(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: cron <delay_ms> [command...]\n");
        printf("  Schedule a delayed command (max 1 min)\n");
        return;
    }
    int delay_ms = atoi(argv[1]);
    if (delay_ms < 100 || delay_ms > 60000) { printf("delay 100-60000 ms\n"); return; }
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 2);
    cron_schedule(cmd, (uint32_t)delay_ms);
    printf("Scheduled in %d ms: %s\n", delay_ms, cmd);
}

static void cmd_bench(int argc, char *argv[]) {
    if (argc < 3) { printf("usage: bench <iters> <command...>\n"); return; }
    int iters = atoi(argv[1]);
    if (iters < 1 || iters > 10000) { printf("iters 1-10000\n"); return; }
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 2);
    bench_run(cmd, iters);
}

static void cmd_time(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: time <command...>\n"); return; }
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 1);
    uint64_t t0 = hal_time_us();
    commands_execute(cmd);
    uint64_t t1 = hal_time_us();
    uint64_t dt = t1 - t0;
    printf("time: %llu us (%.3f ms)\n", dt, dt / 1000.0);
}

#define MAX_ALIASES 64
static char *s_alias_names[MAX_ALIASES];
static char *s_alias_cmds[MAX_ALIASES];
static int s_alias_count = 0;

static void alias_add(const char *name, const char *cmd) {
    for (int i = 0; i < s_alias_count; i++) {
        if (strcmp(s_alias_names[i], name) == 0) {
            free(s_alias_cmds[i]);
            s_alias_cmds[i] = strdup(cmd);
            return;
        }
    }
    if (s_alias_count >= MAX_ALIASES) { printf("max aliases (%d) reached\n", MAX_ALIASES); return; }
    s_alias_names[s_alias_count] = strdup(name);
    s_alias_cmds[s_alias_count] = strdup(cmd);
    s_alias_count++;
}

const char *alias_lookup(const char *name) {
    for (int i = 0; i < s_alias_count; i++)
        if (strcmp(s_alias_names[i], name) == 0) return s_alias_cmds[i];
    return NULL;
}

static void cmd_alias(int argc, char *argv[]) {
    if (argc == 1) {
        for (int i = 0; i < s_alias_count; i++)
            printf("alias %s='%s'\n", s_alias_names[i], s_alias_cmds[i]);
        return;
    }
    if (argc == 2) {
        const char *v = alias_lookup(argv[1]);
        if (v) printf("alias %s='%s'\n", argv[1], v);
        else printf("no such alias: %s\n", argv[1]);
        return;
    }
    char cmd[256]; argv_join(cmd, sizeof(cmd), argc, argv, 2);
    alias_add(argv[1], cmd);
}

static void cmd_unalias(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: unalias <name>\n"); return; }
    for (int i = 0; i < s_alias_count; i++) {
        if (strcmp(s_alias_names[i], argv[1]) == 0) {
            free(s_alias_names[i]); free(s_alias_cmds[i]);
            s_alias_names[i] = s_alias_names[s_alias_count - 1];
            s_alias_cmds[i] = s_alias_cmds[s_alias_count - 1];
            s_alias_count--;
            printf("removed alias '%s'\n", argv[1]);
            return;
        }
    }
    printf("no such alias: %s\n", argv[1]);
}

static void cmd_power(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("VSYS: %.2f V  (ESP32 VBUS monitor)\n", 5.0);
    printf("Battery sense: N/A\n");
}

static void cmd_free(int argc, char *argv[]) {
    (void)argc; (void)argv;
    heap_track_report();
}

static void cmd_sysinfo(int argc, char *argv[]) {
    (void)argc; (void)argv;
    const board_info_t *b = board_detect();
    printf("========================================\n");
    printf("DeckOS v3.0\n");
    printf("Board: %s\n", b->name);
    printf("Build: %s %s\n", __DATE__, __TIME__);
    printf("uptime: ");
    print_uptime();
    printf("\n");
    printf("Flash: %lu KB | PSRAM: %s | SD: %s | CAM: %s\n",
        b->flash_kb, b->has_psram ? "Y" : "N", b->has_sdcard ? "Y" : "N", b->has_camera ? "Y" : "N");
    printf("Commands issued: %lu, unknown: %lu\n", s_cmd_count, s_unknown_count);
    printf("========================================\n");
}

static void cmd_uname(int argc, char *argv[]) {
    bool all = (argc > 1 && strcmp(argv[1], "-a") == 0);
    const board_info_t *b = board_detect();
    printf("DeckOS v3.0 %s", b->name);
    if (all) printf(" xtensa-esp32");
    printf("\n");
}

static void cmd_rand(int argc, char *argv[]) {
    int minv = (argc >= 2) ? atoi(argv[1]) : 0;
    int maxv = (argc >= 3) ? atoi(argv[2]) : RAND_MAX;
    if (minv > maxv) { int t = minv; minv = maxv; maxv = t; }
    if (minv == maxv) { printf("%d\n", minv); return; }
    int r = rand();
    r = minv + r % (maxv - minv + 1);
    printf("%d\n", r);
}

static void cmd_reboot(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("rebooting...\n");
    fflush(stdout);
    hal_sleep_ms(100);
    hal_reboot();
}

static void cmd_drivers(int argc, char *argv[]) {
    (void)argc; (void)argv;
    drivers_list();
}

static void cmd_tasks(int argc, char *argv[]) {
    if (argc < 2) {
        sched_list();
        return;
    }
    if (strcmp(argv[1], "list") == 0) sched_list();
    else if (strcmp(argv[1], "enable") == 0) {
        if (argc < 3) { printf("need task id\n"); return; }
        sched_enable(atoi(argv[2]), true);
    } else if (strcmp(argv[1], "disable") == 0) {
        if (argc < 3) { printf("need task id\n"); return; }
        sched_enable(atoi(argv[2]), false);
    } else printf("unknown: %s\n", argv[1]);
}

static void cmd_config(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: config show|set <key> <val>|save|reset\n"); return; }
    flash_config_t *cfg = &g_config;
    if (strcmp(argv[1], "show") == 0) {
        config_print(cfg);
    } else if (strcmp(argv[1], "save") == 0) {
        config_save(cfg);
    } else if (strcmp(argv[1], "reset") == 0) {
        config_defaults(cfg);
        config_save(cfg);
        printf("config reset to defaults\n");
    } else printf("unknown config subcommand\n");
}

static void cmd_syslog(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: syslog show|warn|err|write <msg>|clear|stats\n"); return; }
    if (strcmp(argv[1], "show") == 0) {
        syslog_dump(LOG_INFO, 50);
    } else if (strcmp(argv[1], "warn") == 0) {
        const char *msg = (argc >= 3) ? argv[2] : "manual warning";
        LOG_W("cmd", msg);
        printf("syslog warning written\n");
    } else if (strcmp(argv[1], "err") == 0) {
        const char *msg = (argc >= 3) ? argv[2] : "manual error";
        LOG_E("cmd", msg);
        printf("syslog error written\n");
    } else if (strcmp(argv[1], "write") == 0) {
        if (argc < 3) { printf("need message\n"); return; }
        LOG_I("cmd", argv[2]);
        printf("syslog info written\n");
    } else if (strcmp(argv[1], "clear") == 0) {
        syslog_clear();
    } else if (strcmp(argv[1], "stats") == 0) {
        printf("syslog: %lu entries\n", (unsigned long)syslog_total());
    } else printf("unknown syslog: %s\n", argv[1]);
}

static void cmd_jobs(int argc, char *argv[]) {
    (void)argc; (void)argv;
    bg_job_list();
}

static void cmd_date(int argc, char *argv[]) {
    if (argc >= 7) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = atoi(argv[1]) - 1900;
        tm.tm_mon  = atoi(argv[2]) - 1;
        tm.tm_mday = atoi(argv[3]);
        tm.tm_hour = atoi(argv[4]);
        tm.tm_min  = atoi(argv[5]);
        tm.tm_sec  = atoi(argv[6]);
        time_t t = mktime(&tm);
        if (t < 0) { printf("invalid date\n"); return; }
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        printf("RTC set to %04d-%02d-%02d %02d:%02d:%02d\n",
            atoi(argv[1]), atoi(argv[2]), atoi(argv[3]),
            atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        return;
    }
    time_t now;
    time(&now);
    struct tm *lt = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
    printf("RTC: %s\n", buf);
}

static void cmd_history(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("history: (built-in shell history not yet available through this command)\n");
}

static void cmd_stats(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("Runtime statistics:\n");
    printf("  commands issued : %lu\n", s_cmd_count);
    printf("  unknown         : %lu\n", s_unknown_count);
    printf("  missed tasks    : (check syslog)\n");
}

static void cmd_top(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("=== Top ===\n");
    printf("uptime: "); print_uptime(); printf("\n");
    heap_track_report();
    sched_list();
}

static void cmd_uart(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  uart passthrough <baud> <tx> <rx> [timeout_s]\n");
        return;
    }
    if (strcmp(argv[1], "passthrough") == 0) {
        if (argc < 5) { printf("usage: uart passthrough <baud> <tx> <rx> [timeout_s]\n"); return; }
        int baud = atoi(argv[2]);
        int tx = atoi(argv[3]);
        int rx = atoi(argv[4]);
        int timeout_s = (argc >= 6) ? atoi(argv[5]) : 30;
        uart_passthrough((uint)tx, (uint)rx, (uint)baud, (uint32_t)(timeout_s * 1000));
    } else printf("unknown uart: %s\n", argv[1]);
}

// esp32 specific commands

static void cmd_board(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("Board: %s\n", hal_board_name());
    printf("Chip: ESP32\n");
    printf("Flash: %lu KB\n", (unsigned long)(hal_board_flash_size() / 1024));
    printf("PSRAM: %lu KB\n", (unsigned long)(hal_board_psram_size() / 1024));
    printf("Camera: %s\n", hal_board_has_camera() ? "yes" : "no");
    printf("SD Card: %s\n", hal_board_has_sdcard() ? "yes" : "no");
}

static void cmd_psram(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint32_t total = hal_board_psram_size();
    printf("PSRAM total: %lu KB\n", (unsigned long)(total / 1024));
    if (total > 0) {
        printf("PSRAM free: %lu KB\n",
               (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        printf("PSRAM largest: %lu KB\n",
               (unsigned long)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
    }
}

// Camera (ESP32-CAM)
#include "camera_esp32.h"
#include "wifi.h"

static void cmd_camera(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  camera init                  init the camera\n");
        printf("  camera capture               capture and dump JPEG info\n");
        printf("  camera save [path]           save capture to SPIFFS\n");
        printf("  camera ls                    list saved captures\n");
        printf("  camera info                  show camera info\n");
        printf("  camera res <qqvga|qvga|vga|svga|xga|sxga|uxga>\n");
        printf("  camera quality <10-63>       set JPEG quality\n");
        printf("  camera brightness <-2..2>    set brightness\n");
        printf("  camera contrast <-2..2>      set contrast\n");
        printf("  camera stream                start MJPEG stream on port 80\n");
        printf("  camera stream stop           stop MJPEG stream\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (deck_cam_init()) printf("Camera initialised\n");
        else printf("Camera init FAILED\n");
    } else if (strcmp(argv[1], "capture") == 0) {
        cam_frame_t f;
        if (deck_cam_capture(&f)) {
            printf("captured %dx%d %s (%zu bytes)\n",
                   f.width, f.height, f.jpeg ? "JPEG" : "RAW", f.len);
            deck_cam_return_frame(&f);
        } else printf("capture failed\n");
    } else if (strcmp(argv[1], "ls") == 0) {
        deck_cam_list();
    } else if (strcmp(argv[1], "save") == 0) {
        if (!deck_cam_is_ready()) { printf("camera not initialised\n"); return; }
        if (argc >= 3) {
            deck_cam_save(argv[2]);
        } else {
            deck_cam_save_auto();
        }
    } else if (strcmp(argv[1], "info") == 0) {
        deck_cam_print_info();
    } else if (strcmp(argv[1], "res") == 0 && argc >= 3) {
        const char* res_str[] = {"qqvga","qvga","vga","svga","xga","sxga","uxga"};
        for (int i = 0; i < 7; i++) {
            if (strcmp(argv[2], res_str[i]) == 0) {
                deck_cam_set_resolution((cam_resolution_t)i);
                printf("resolution set to %s\n", res_str[i]);
                return;
            }
        }
        printf("unknown resolution\n");
    } else if (strcmp(argv[1], "quality") == 0 && argc >= 3) {
        deck_cam_set_quality((uint8_t)atoi(argv[2]));
        printf("quality set\n");
    } else if (strcmp(argv[1], "brightness") == 0 && argc >= 3) {
        deck_cam_set_brightness((int8_t)atoi(argv[2]));
        printf("brightness set\n");
    } else if (strcmp(argv[1], "contrast") == 0 && argc >= 3) {
        deck_cam_set_contrast((int8_t)atoi(argv[2]));
        printf("contrast set\n");
    } else if (strcmp(argv[1], "stream") == 0) {
        if (argc >= 3 && strcmp(argv[2], "stop") == 0) {
            deck_cam_stream_stop();
            printf("stream stopped\n");
        } else if (deck_cam_stream_start()) {
            printf("MJPEG stream at http://%s/\n", wifi_get_ip());
        } else {
            printf("stream start failed\n");
        }
    } else printf("unknown camera: %s\n", argv[1]);
}

// WiFi (native ESP32)
#include "wifi.h"

static void cmd_wifi(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  wifi init                    init WiFi subsystem\n");
        printf("  wifi ap <ssid> [pass]        start softAP mode\n");
        printf("  wifi scan                    scan for networks\n");
        printf("  wifi join <ssid> <pass>      connect to network\n");
        printf("  wifi disconnect              disconnect\n");
        printf("  wifi status                  show connection state\n");
        printf("  wifi get <url>               HTTP GET\n");
        printf("  wifi post <url> <body>       HTTP POST\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (wifi_init()) printf("WiFi initialised\n");
        else printf("WiFi init FAILED\n");
    } else if (strcmp(argv[1], "ap") == 0 && argc >= 3) {
        const char* pass = (argc >= 4) ? argv[3] : "";
        if (wifi_ap_start(argv[2], pass)) {
            printf("AP '%s' started at 192.168.4.1\n", argv[2]);
        } else printf("AP start failed\n");
    } else if (strcmp(argv[1], "scan") == 0) {
        wifi_ap_t aps[20];
        int count = 0;
        if (wifi_scan(aps, 20, &count)) {
            printf("Networks found: %d\n", count);
            for (int i = 0; i < count; i++) {
                printf("  %-32s  RSSI: %4d dBm\n", aps[i].ssid, aps[i].rssi);
            }
        } else printf("scan failed\n");
    } else if (strcmp(argv[1], "join") == 0 && argc >= 4) {
        if (wifi_connect(argv[2], argv[3])) {
            printf("Connected! IP: %s\n", wifi_get_ip());
        } else printf("Connection failed\n");
    } else if (strcmp(argv[1], "disconnect") == 0) {
        wifi_disconnect();
        printf("Disconnected\n");
    } else if (strcmp(argv[1], "status") == 0) {
        wifi_print_status();
    } else if (strcmp(argv[1], "get") == 0 && argc >= 3) {
        char resp[2048];
        if (wifi_http_get(argv[2], resp, sizeof(resp))) {
            printf("%s\n", resp);
        } else printf("HTTP GET failed\n");
    } else if (strcmp(argv[1], "post") == 0 && argc >= 4) {
        char resp[2048];
        if (wifi_http_post(argv[2], argv[3], resp, sizeof(resp))) {
            printf("%s\n", resp);
        } else printf("HTTP POST failed\n");
    } else printf("unknown wifi: %s\n", argv[1]);
}

// Bluetooth (native ESP32)

static void cmd_bt(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  bt init              init Bluetooth (SPP)\n");
        printf("  bt deinit            power down Bluetooth\n");
        printf("  bt status            show connection state\n");
        printf("  bt shell             interactive BT shell\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (bt_init(115200)) printf("Bluetooth initialised\n");
        else printf("BT init FAILED\n");
    } else if (strcmp(argv[1], "deinit") == 0) {
        bt_deinit();
        printf("Bluetooth deinitialised\n");
    } else if (strcmp(argv[1], "status") == 0) {
        printf("BT: %s\n", bt_is_connected() ? "connected" : "disconnected");
    } else if (strcmp(argv[1], "shell") == 0) {
        bt_shell();
    } else printf("unknown bt: %s\n", argv[1]);
}

// Swarm / ESP-NOW
static void cmd_swarm(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  swarm init             - start ESP-NOW mesh\n");
        printf("  swarm id <name>        - set this node's name\n");
        printf("  swarm mac              - show this node's MAC\n");
        printf("  swarm peer <MAC>       - register a peer node\n");
        printf("  swarm pub <lat> <lon> <alt> <hdg> <state>  - broadcast position\n");
        printf("  swarm list             - show all known peers\n");
        printf("  swarm stop             - stop the mesh\n");
        return;
    }

    if (strcmp(argv[1], "init") == 0) {
        if (swarm_init()) printf("swarm: ESP-NOW mesh started\n");
        else printf("swarm: init FAILED\n");
    } else if (strcmp(argv[1], "id") == 0 && argc >= 3) {
        swarm_set_id(argv[2]);
    } else if (strcmp(argv[1], "mac") == 0) {
        const char *mac = swarm_get_mac_str();
        if (mac && mac[0]) printf("swarm MAC: %s\n", mac);
        else printf("swarm: not initialised\n");
    } else if (strcmp(argv[1], "peer") == 0 && argc >= 3) {
        swarm_add_peer(argv[2]);
    } else if (strcmp(argv[1], "pub") == 0 && argc >= 7) {
        float lat = atof(argv[2]);
        float lon = atof(argv[3]);
        float alt = atof(argv[4]);
        float hdg = atof(argv[5]);
        int state = atoi(argv[6]);
        if (!swarm_is_active()) {
            printf("swarm: not initialised - run 'swarm init' first\n");
            return;
        }
        swarm_publish(lat, lon, alt, hdg, (uint8_t)state);
    } else if (strcmp(argv[1], "list") == 0) {
        if (!swarm_is_active()) {
            printf("swarm: not initialised\n");
            return;
        }
        int n = swarm_peer_count();
        printf("swarm peers (%d):\n", n);
        for (int i = 0; i < n; i++) {
            const swarm_peer_t *p = swarm_get_peer(i);
            if (p) {
                printf("  %d. %02X:%02X:%02X:%02X:%02X:%02X  %s\n",
                       i + 1,
                       p->mac[0], p->mac[1], p->mac[2],
                       p->mac[3], p->mac[4], p->mac[5],
                       p->name);
                if (p->last_seen > 0) {
                    printf("     lat=%.4f lon=%.4f alt=%.1f hdg=%.1f state=%d last_seen=%lu\n",
                           p->lat, p->lon, p->alt, p->hdg, p->state,
                           (unsigned long)p->last_seen);
                }
            }
        }
    } else if (strcmp(argv[1], "stop") == 0) {
        swarm_stop();
        } else {
            printf("swarm: unknown subcommand '%s'\n", argv[1]);
        }
    }

static void cmd_edit(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: edit <file>\n"); return; }
    if (!module_is_loaded("editor")) {
        printf("edit: editor module not loaded. Run 'module load editor' first\n");
        return;
    }
    editor_run(argv[1]);
}

static void cmd_module(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: module load|unload|list <name>\n"); return; }
    if (strcmp(argv[1], "list") == 0) {
        int n = module_total_count();
        printf("modules:\n");
        printf("  %-10s %-6s %-7s  %s\n", "name", "state", "ram", "description");
        uint32_t loaded_ram = 0;
        for (int i = 0; i < n; i++) {
            const module_t *m = module_total_get(i);
            if (!m) continue;
            printf("  %-10s %-6s %4lu KB  %s\n",
                m->name,
                m->loaded ? "LOADED" : "-",
                (unsigned long)(m->ram_bytes / 1024),
                m->description);
            if (m->loaded) loaded_ram += m->ram_bytes;
        }
        printf("  ----\n  loaded RAM: %lu KB\n", (unsigned long)(loaded_ram / 1024));
        printf("  usage: module load <name> | module unload <name> | module list\n");
    } else if (strcmp(argv[1], "load") == 0 && argc >= 3) {
        module_load(argv[2]);
    } else if (strcmp(argv[1], "unload") == 0 && argc >= 3) {
        module_unload(argv[2]);
    } else printf("unknown module subcommand\n");
}

static void cmd_save(int argc, char *argv[]) {
    (void)argc; (void)argv;
    vfs_save();
    printf("VFS saved\n");
}

static void cmd_script(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: script <file>\n"); return; }
    script_run_file(argv[1]);
}

static void cmd_run(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: run <command...>\n"); return; }
    char line[SCRIPT_LINE_LEN * 2];
    line[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) strncat(line, " ", sizeof(line) - strlen(line) - 1);
        strncat(line, argv[i], sizeof(line) - strlen(line) - 1);
    }
    script_ctx_t ctx;
    script_ctx_init(&ctx);
    script_run_string(&ctx, line);
}

static void cmd_flash(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: flash read <addr_hex> <len> | write <addr_hex> <hex_bytes...> | erase <sector_hex>\n");
        return;
    }
    if (strcmp(argv[1], "read") == 0 && argc >= 4) {
        uint32_t addr = (uint32_t)strtoul(argv[2], NULL, 16);
        uint32_t len = (uint32_t)atoi(argv[3]);
        if (len > 1024) { printf("max 1024 bytes\n"); return; }
        uint8_t *buf = malloc(len);
        if (!buf) { printf("OOM\n"); return; }
        if (esp_flash_read(NULL, buf, addr, len) == ESP_OK) {
            for (uint32_t i = 0; i < len; i++) {
                printf("%02X ", buf[i]);
                if ((i + 1) % 16 == 0) printf("\n");
            }
            if (len % 16) printf("\n");
        } else printf("flash read failed\n");
        free(buf);
    } else if (strcmp(argv[1], "write") == 0 && argc >= 4) {
        uint32_t addr = (uint32_t)strtoul(argv[2], NULL, 16);
        uint8_t data[64]; int dlen = 0;
        for (int a = 3; a < argc && dlen < 64; a++) {
            unsigned long v = strtoul(argv[a], NULL, 16);
            if (v > 0xFF) { printf("byte value 0-255\n"); return; }
            data[dlen++] = (uint8_t)v;
        }
        if (esp_flash_write(NULL, data, addr, (uint32_t)dlen) == ESP_OK)
            printf("wrote %d bytes to 0x%08lX\n", dlen, (unsigned long)addr);
        else printf("flash write failed\n");
    } else if (strcmp(argv[1], "erase") == 0 && argc >= 3) {
        uint32_t sector = (uint32_t)strtoul(argv[2], NULL, 16);
        if (esp_flash_erase_region(NULL, sector * 4096, 4096) == ESP_OK)
            printf("erased sector 0x%08lX\n", (unsigned long)sector);
        else printf("flash erase failed\n");
    } else printf("unknown flash subcommand\n");
}

static void cmd_stack(int argc, char *argv[]) {
    (void)argc; (void)argv;
    UBaseType_t high = uxTaskGetStackHighWaterMark(NULL);
    printf("current task stack high water mark: %u words (%u bytes)\n",
           (unsigned)high, (unsigned)(high * 4));
}

static void cmd_clock(int argc, char *argv[]) {
    (void)argc; (void)argv;
#if CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
    printf("CPU frequency: %d MHz\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
#else
    printf("CPU frequency: unknown\n");
#endif
}

static void cmd_uid(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    printf("UID (MAC): %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void cmd_fault(int argc, char *argv[]) {
    (void)argc; (void)argv;
    esp_reset_reason_t reason = esp_reset_reason();
    printf("fault info:\n");
    printf("  reset reason : %d", reason);
    switch (reason) {
        case ESP_RST_UNKNOWN:   printf(" (unknown)\n"); break;
        case ESP_RST_POWERON:   printf(" (power-on)\n"); break;
        case ESP_RST_EXT:       printf(" (external pin)\n"); break;
        case ESP_RST_SW:        printf(" (software reset)\n"); break;
        case ESP_RST_PANIC:     printf(" (PANIC / abort)\n"); break;
        case ESP_RST_INT_WDT:   printf(" (interrupt watchdog)\n"); break;
        case ESP_RST_TASK_WDT:  printf(" (task watchdog)\n"); break;
        case ESP_RST_WDT:       printf(" (other watchdog)\n"); break;
        case ESP_RST_DEEPSLEEP: printf(" (deep sleep exit)\n"); break;
        case ESP_RST_BROWNOUT:  printf(" (brownout)\n"); break;
        case ESP_RST_SDIO:      printf(" (SDIO)\n"); break;
        default:                printf("\n"); break;
    }
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if (task) printf("  current task: %s\n", pcTaskGetName(task));
    printf("  uptime       : %lu ms\n", (unsigned long)(hal_time_us() / 1000));
    printf("  free heap    : %lu bytes\n", (unsigned long)esp_get_free_heap_size());
#if CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
    printf("  CPU freq     : %d MHz\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
#endif
}

typedef void (*cmd_func_t)(int argc, char *argv[]);
typedef struct { const char *name; cmd_func_t func; } cmd_entry_t;

#define MAX_DYNAMIC_CMDS 32
static cmd_entry_t s_cmd_dynamic[MAX_DYNAMIC_CMDS];
static int s_cmd_dynamic_count = 0;

void commands_api_register(const char *name, const char *desc, void (*handler)(int, char**)) {
    (void)desc;
    if (s_cmd_dynamic_count >= MAX_DYNAMIC_CMDS) {
        printf("cmd_api: max dynamic commands (%d) reached\n", MAX_DYNAMIC_CMDS);
        return;
    }
    s_cmd_dynamic[s_cmd_dynamic_count].name = name;
    s_cmd_dynamic[s_cmd_dynamic_count].func = handler;
    s_cmd_dynamic_count++;
}

void commands_api_unregister(const char *name) {
    for (int i = 0; i < s_cmd_dynamic_count; i++) {
        if (strcmp(s_cmd_dynamic[i].name, name) == 0) {
            s_cmd_dynamic[i] = s_cmd_dynamic[--s_cmd_dynamic_count];
            return;
        }
    }
}

static int cmd_func_compare(const void *a, const void *b) {
    return strcmp(((const cmd_entry_t*)a)->name, ((const cmd_entry_t*)b)->name);
}

static const cmd_entry_t s_cmd_table[] = {
    {"adc",       cmd_adc},
    {"alias",     cmd_alias},
    {"append",    cmd_append},
    {"avg",       cmd_avg},
    {"bench",     cmd_bench},
    {"board",     cmd_board},
    {"cat",       cmd_cat},
    {"cd",        cmd_cd},
    {"clear",     cmd_clear},
    {"clock",     cmd_clock},
    {"config",    cmd_config},
    {"console",   cmd_console},
    {"cp",        cmd_cp},
    {"cron",      cmd_cron},
    {"date",      cmd_date},
    {"detect",    cmd_detect},
    {"df",        cmd_df},
    {"drivers",   cmd_drivers},
    {"echo",      cmd_echo},
    {"edit",      cmd_edit},
    {"fault",     cmd_fault},
    {"find",      cmd_find},
    {"flash",     cmd_flash},
    {"free",      cmd_free},
    {"gpio",      cmd_gpio},
    {"grep",      cmd_grep},
    {"help",      cmd_help},
    {"hexdump",   cmd_hexdump},
    {"hid",       cmd_hid},
    {"history",   cmd_history},
    {"i2c",       cmd_i2c},
    {"iwrite",    cmd_iwrite},
    {"jobs",      cmd_jobs},
    {"la",        cmd_la},
    {"led",       cmd_led},
    {"ls",        cmd_ls},
    {"mem",       cmd_mem},
    {"mkdir",     cmd_mkdir},
    {"module",    cmd_module},
    {"mv",        cmd_mv},
    {"pin",       cmd_pin},
    {"power",     cmd_power},
    {"psram",     cmd_psram},
    {"pull",      cmd_pull},
    {"pwd",       cmd_pwd},
    {"pwm",       cmd_pwm},
    {"rand",      cmd_rand},
    {"reboot",    cmd_reboot},
    {"repeat",    cmd_repeat},
    {"rm",        cmd_rm},
    {"run",       cmd_run},
    {"save",      cmd_save},
    {"scope",     cmd_scope},
    {"script",    cmd_script},
    {"sleep",     cmd_sleep},
    {"spi",       cmd_spi},
    {"stack",     cmd_stack},
    {"stat",      cmd_stat},
    {"stats",     cmd_stats},
    {"sysinfo",   cmd_sysinfo},
    {"syslog",    cmd_syslog},
    {"tasks",     cmd_tasks},
    {"temp",      cmd_temp},
    {"time",      cmd_time},
    {"top",       cmd_top},
    {"touch",     cmd_touch},
    {"tree",      cmd_tree},
    {"trigger",   cmd_trigger},
    {"uart",      cmd_uart},
    {"uid",       cmd_uid},
    {"unalias",   cmd_unalias},
    {"uname",     cmd_uname},
    {"uptime",    cmd_uptime},
    {"usb",       cmd_usb},
    {"version",   cmd_version},
    {"watch",     cmd_watch},
    {"wc",        cmd_wc},
    {"wdog",      cmd_wdog},
    {"write",     cmd_write},
    {"ota",       cmd_ota},
    {"dashboard", cmd_dashboard},
};

static const int s_cmd_count_tbl = sizeof(s_cmd_table) / sizeof(s_cmd_table[0]);

bool commands_execute(const char *line) {
    if (!line || line[0] == '\0' || line[0] == '#') return true;
    s_cmd_count++;

    char buf[384];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int argc = 0;
    char *argv_[64];
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t') *p++ = '\0';
        if (!*p) break;
        if (*p == '"') {
            argv_[argc++] = ++p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            argv_[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
        if (argc >= 64) break;
    }

    if (argc == 0) return true;

    const char *alias = alias_lookup(argv_[0]);
    if (alias) {
        char expanded[384];
        snprintf(expanded, sizeof(expanded), "%s %s", alias, buf + strlen(argv_[0]));
        while (*expanded && (*expanded == ' ' || *expanded == '\t'));
        return commands_execute(expanded);
    }

    cmd_entry_t key = {argv_[0], NULL};
    const cmd_entry_t *found = (const cmd_entry_t *)bsearch(&key, s_cmd_table, s_cmd_count_tbl,
        sizeof(cmd_entry_t), cmd_func_compare);
    if (found) {
        found->func(argc, argv_);
        return true;
    }

    for (int i = 0; i < s_cmd_dynamic_count; i++) {
        if (strcmp(s_cmd_dynamic[i].name, argv_[0]) == 0) {
            s_cmd_dynamic[i].func(argc, argv_);
            return true;
        }
    }

    s_unknown_count++;
    printf("unknown command: %s\n", argv_[0]);
    return false;
}

void commands_init(void) {
    s_boot_us = hal_time_us();
    srand((unsigned)hal_time_us());
    module_set_cmd_api(commands_api_register, commands_api_unregister);
    printf("commands initted (%d commands + %d dynamic slots)\n", s_cmd_count_tbl, MAX_DYNAMIC_CMDS);
}

void commands_list(void) {
    for (int i = 0; i < s_cmd_count_tbl; i++)
        printf("%s ", s_cmd_table[i].name);
    printf("\n");
}
