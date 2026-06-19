# DeckOS for ESP32

A port of [DeckOS](https://github.com/enginestein/DeckOS) — a bare-metal embedded shell/OS originally written for the **Raspberry Pi Pico (RP2040)** — now running on the **ESP32** under **ESP-IDF / FreeRTOS**.

This port introduces a **Hardware Abstraction Layer (HAL)** that decouples the kernel, shell, scripting, and command logic from the MCU hardware, allowing the same high-level codebase to target both the RP2040 and ESP32.

```
+----------------------------------------------+
|  Shell / Commands / DeckScript / VFS         |
|  command_table[] (built-in) + s_dynamic[]    |
|  (dynamic commands from loaded modules)      |
+----------------------------------------------+
|  Module Registry (s_modules[] + plugins)     |
|  load/unload lifecycle, event dispatch       |
+----------------------------------------------+
|  Kernel / Scheduler / Syslog / Config        |
+----------------------------------------------+
|  HAL (GPIO, I2C, SPI, ADC, PWM, UART)        |
+----------------------------------------------+
|  ESP-IDF / FreeRTOS / ESP32 Hardware          |
+----------------------------------------------+
```

---

## Table of Contents

- [DeckOS for ESP32](#deckos-for-esp32)
  - [Table of Contents](#table-of-contents)
  - [Differences from the RP2040 version](#differences-from-the-rp2040-version)
    - [New in this port](#new-in-this-port)
  - [What you need](#what-you-need)
  - [Getting started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Build and flash](#build-and-flash)
    - [Monitor](#monitor)
  - [Architecture](#architecture)
    - [Boot order](#boot-order)
  - [AutoRun](#autorun)
    - [How it works](#how-it-works)
    - [Customization](#customization)
  - [Module System](#module-system)
    - [Module Commands](#module-commands)
    - [Available Modules](#available-modules)
    - [Usage](#usage)
    - [Plugin API](#plugin-api)
    - [Event System](#event-system)
    - [Dynamic Command Registration](#dynamic-command-registration)
  - [Commands](#commands)
    - [Core / Info](#core--info)
    - [Hardware](#hardware)
    - [Buses](#buses)
    - [Probes \& Analysis](#probes--analysis)
    - [Servo](#servo)
    - [Audio \& Signalling](#audio--signalling)
    - [Scripting \& Automation](#scripting--automation)
    - [Editor](#editor)
    - [System](#system)
    - [Filesystem](#filesystem)
    - [WiFi](#wifi)
    - [Swarm / ESP-NOW](#swarm--esp-now)
    - [Camera](#camera)
    - [NRF24L01](#nrf24l01)
    - [Bluetooth (stubbed)](#bluetooth-stubbed)
  - [Shell](#shell)
    - [Keyboard shortcuts](#keyboard-shortcuts)
  - [Filesystem (VFS)](#filesystem-vfs)
    - [VFS layout](#vfs-layout)
    - [File persistence](#file-persistence)
  - [DeckScript](#deckscript)
    - [Features](#features)
    - [Example](#example)
    - [Limits](#limits)
  - [WiFi](#wifi-1)
    - [Station mode](#station-mode)
    - [SoftAP mode](#softap-mode)
    - [HTTP client](#http-client)
  - [ESP-NOW Swarm](#esp-now-swarm)
    - [Features](#features-1)
  - [Camera (ESP32-CAM)](#camera-esp32-cam)
    - [Pin mapping (ESP32-CAM default)](#pin-mapping-esp32-cam-default)
    - [Usage](#usage-1)
    - [Resolutions](#resolutions)
  - [Web Dashboard](#web-dashboard)
    - [Dashboard features](#dashboard-features)
  - [ESP-NOW RC Bridge](#esp-now-rc-bridge)
    - [Architecture](#architecture-1)
    - [Setup](#setup)
  - [NRF24L01 Radio](#nrf24l01-radio)
    - [Features](#features-2)
  - [Bluetooth](#bluetooth)
  - [OLED (SSD1306)](#oled-ssd1306)
    - [Commands](#commands-1)
    - [OLED console mirror](#oled-console-mirror)
  - [Servo](#servo-1)
  - [Audio](#audio)
  - [IMU (MPU6050)](#imu-mpu6050)
  - [Modules](#modules)
    - [Text Editor (`edit`)](#text-editor-edit)
  - [Config system](#config-system)
    - [Config keys](#config-keys)
  - [Syslog](#syslog)
  - [Scheduler](#scheduler)
  - [Drivers](#drivers)
  - [Hardware Abstraction Layer (HAL)](#hardware-abstraction-layer-hal)
  - [Board support](#board-support)
  - [Partition layout](#partition-layout)
  - [Project layout](#project-layout)
  - [Original RP2040 DeckOS](#original-rp2040-deckos)
  - [Demo](#demo)

---

## Differences from the RP2040 version

| Aspect | RP2040 DeckOS | ESP32 DeckOS |
|--------|----------------|--------------|
| **SDK** | Pico SDK (bare metal) | ESP-IDF 5.1 (FreeRTOS) |
| **CPU** | Dual-core Cortex-M0+ @ 125 MHz | Dual-core Xtensa LX6 @ 240 MHz |
| **RAM** | 264 KB SRAM | 520 KB SRAM + up to 8 MB PSRAM |
| **Flash** | 2 MB (XIP) | 4 MB (SPI flash, mapped) |
| **Entry point** | `main()` | `app_main()` -> spawns `kernel_task` on Core 0 |
| **Kernel loop** | Busy-wait `while(1)` | Poll loop with `hal_sleep_ms(1)` (yields to idle) |
| **USB** | TinyUSB (CDC + MSC + HID) | ESP-IDF USB CDC only |
| **WiFi** | External ESP8266 (UART bridge) | **Native ESP-IDF WiFi** |
| **ESP-NOW** | Via ESP8266 bridge | **Native ESP-NOW** |
| **Bluetooth** | External HC-05 (UART) | **Module (BT SPP, ~48 KB)** |
| **Filesystem** | VFS persisted to raw flash | VFS persisted to **SPIFFS** (2 MB partition) |
| **Config** | Last 4 KB of flash (CRC32) | **NVS** (key-value storage) |
| **Camera** | External ESP32-CAM bridge | **Native esp32-camera driver** |
| **Build system** | CMake + Make | `idf.py build` / `idf.py flash` |
| **Toolchain** | `arm-none-eabi-gcc` | `xtensa-esp32-elf-gcc` |

### New in this port

- **Native WiFi** — station + AP mode, HTTP GET/POST, auto-reconnect
- **Native ESP-NOW** — peer-to-peer mesh telemetry (20 peers)
- **ESP32-CAM support** — OV2640 camera init, capture, MJPEG stream on port 81, SPIFFS save
- **NRF24L01+ radio driver** — 2.4 GHz SPI radio with TX/RX, 6 pipes, carrier scan
- **SPIFFS persistence** — 2 MB partition for VFS files
- **NVS config** — key-value storage via ESP-IDF NVS
- **PSRAM support** — auto-detected and heap-allocated via `SPIRAM_USE_CAPS_ALLOC`
- **Board auto-detection** — ESP32-WROOM vs ESP32-CAM vs ESP32-S3
- **Hardware Abstraction Layer** — clean platform abstraction for all peripherals
- **Dynamic command registration** — modules inject commands at runtime
- **Module system** — loadable modules with lifecycle events
- **DeckScript** — built-in scripting language with full control flow
- **Editor module** — nano-style text editor (edit command)
- **VFS save** — persist VFS files to SPIFFS
- **Flash access** — raw flash read/write commands
- **Stack monitoring** — FreeRTOS task stack high-water mark
- **Clock frequency** — CPU speed readout
- **Unique ID** — MAC-based unique identifier
- **Fault handler** — panic info display

---

## What you need

| Thing | Detail |
|---|---|
| Board | ESP32-WROOM, ESP32-CAM, ESP32-S3 (or any generic ESP32) |
| Connection | USB to UART bridge (115200 baud, 8N1) |
| Flash | 4 MB (or larger with partition table adjustment) |
| Optional | PSRAM (auto-detected, used for heap) |
| Optional | SSD1306 OLED on I²C (default GP21 SDA, GP22 SCL) |
| Optional | OV2640 camera module (ESP32-CAM boards) |
| Optional | NRF24L01+ on SPI (default GP18 SCK, GP23 MOSI, GP19 MISO) |
| Optional | MPU6050 IMU on I²C |
| Optional | Servo on any GPIO |
| Optional | Passive buzzer on any GPIO |

---

## Getting started

### Prerequisites

- [ESP-IDF](https://github.com/espressif/esp-idf) v5.1 or newer
- xtensa-esp32-elf toolchain

### Build and flash

```bash
git clone https://github.com/enginestein/DeckOS_ESP32
cd DeckOS_ESP32
idf.py set-target esp32
idf.py build
idf.py -p <port> flash monitor
```

The boot banner and shell prompt will appear on the serial console at 115200 baud.


### Monitor

```bash
idf.py -p <port> monitor
# or
screen <port> 115200
# or
minicom -b 115200 -D <port>
```

---

## Architecture

DeckOS runs as a **FreeRTOS task pinned to Core 0**. The scheduler runs on **Core 1** as a separate FreeRTOS task.

```
app_main()  [Core 0]
  ├── hal_nvs_init()
  ├── hal_spiffs_init()
  └── xTaskCreate(kernel_task, Core 0)

kernel_task  [Core 0]
  ├── hal_console_init()
  ├── hal_board_detect()
  ├── kernel_init()
  │     ├── bootloader_run()     (NVS config load, banner)
  │     ├── vfs_load()           (SPIFFS restore)
  │     ├── drivers_init_all()
  │     ├── sched_init()         (launches Core 1 task)
  │     └── shell_init()
  └── kernel_run()
        └── [cron_poll -> pending_commands_poll -> shell_run -> hal_sleep_ms(1)]

sched task  [Core 1]
  └── [task dispatch -> servo_bg_tick -> bg_job_tick -> hal_sleep_us(1000)]
```

### Boot order

```
hal init -> NVS -> SPIFFS -> board detect -> kernel_init()
  ├── syslog_init()
  ├── bootloader_run()    (banner, config)
  ├── vfs_load()          (restore VFS from SPIFFS)
  ├── drivers_init_all()  (ADC, GPIO, PWM, I2C0, SPI0)
  ├── sched_init()        (FreeRTOS task on Core 1)
  ├── modules_init()
  ├── module_set_cmd_api()
  ├── module_fire_event(BOOT_COMPLETE)
  └── shell_init()        (command registration, prompt)
```

---

## AutoRun

DeckOS_ESP32 can automatically execute a DeckScript at boot, enabling
**headless automation** — no serial terminal needed.

### How it works

If the file `/home/autorun.ds` exists in the VFS, it runs automatically on
every boot after the shell is initialized (before the shell prompt appears).
The script has full access to all shell commands (`gpio_write`, `echo`,
`module`, `wifi`, ...) and the full DeckScript language.

On first boot, a default info script is injected into the VFS that types a
message via USB HID and blinks the onboard LED. You can customize or delete it.

Autorun execution happens in `kernel_run()` on the first poll iteration,
before the shell handles any user input. This means **no serial terminal is
required** — plug in the board and the autorun script fires immediately.


### Customization

```bash
# Disable autorun
rm /home/autorun.ds

# Edit the autorun script
module load editor
edit /home/autorun.ds

# Write your own (example: auto-start WiFi and web dashboard)
write /home/autorun.ds "module load wifi
sleep 2000
wifi init
wifi join MySSID MyPassword
sleep 5000
serve start
gpio_write 33 1"
```

> Data persists only after running `save` (VFS → SPIFFS). The autorun.ds file
> is re-injected on first boot if it does not exist, so deleting it is permanent
> unless you re-flash the firmware.

---

## Module System

DeckOS_ESP32 uses a loadable module architecture. Features that carry a RAM
footprint are packaged as **modules** that allocate memory only when loaded.
Community extensions are supported through the **plugin API** — third-party
code can register commands, listen to events, and participate in the module
lifecycle without modifying the core firmware.

### Module Commands

| Command | What it does |
|---|---|
| `module list` | List all modules, their load state, and RAM cost |
| `module load <name>` | Allocate the module's buffers and enable it |
| `module unload <name>` | Free the module's buffers |

### Available Modules

| Module | Description | RAM |
|---|---|---|
| `editor` | Nano-style text editor (edit command) | ~96 KB |
| `wifi` | Native ESP32 WiFi (init/ap/scan/join/disconnect/status) | ~32 KB |
| `bluetooth` | Native ESP32 BT SPP (init/shell/exec/top/send/recv) | ~48 KB |
| `servo` | Servo control (pin angle/sweep/bg) | ~1 KB |
| `oled` | SSD1306 OLED display (init/on/off/clear/text/line/rect) | ~2 KB |
| `swarm` | ESP-NOW mesh (init/id/mac/peer/pub/list/stop) | ~8 KB |
| `nrf24` | NRF24L01+ radio (init/send/listen/status) | ~2 KB |
| `camera` | ESP32-CAM camera (init/capture/save/stream) | ~64 KB |
| `imu` | MPU6050 accelerometer/gyro (read/stream/attitude) | ~2 KB |
| `tone` | Audio tone generation | ~1 KB |
| `morse` | Morse code signalling | ~1 KB |
| `plugin-example` | Example community plugin template | ~1 KB |

### Usage

```bash
> module list
modules:
  name       state  ram      description
  editor     -        96 KB  nano-style text editor (edit command)
  wifi       -        32 KB  Native ESP32 WiFi
  bluetooth  -        48 KB  Bluetooth SPP
  servo      -         2 KB  Servo control
  ...
  loaded RAM: 0 KB
  usage: module load <name> | module unload <name> | module list

> module load wifi
> wifi scan
> module unload wifi
```

### Plugin API

Community plugins register themselves with `module_register_plugin()` and can:

- Register shell commands (auto-registered on load, unregistered on unload)
- Listen for lifecycle events (MODULE_EVENT_BOOT_COMPLETE, TICK, PRE_COMMAND, POST_COMMAND)
- Declare load/unload hooks and semver version

**Writing a plugin:**

```c
// modules/my_plugin.c
#include "module.h"
#include "commands.h"

static void cmd_hello(int argc, char *argv[]) {
    printf("Hello from my plugin!\n");
}

static module_cmd_t s_cmds[] = {
    {"hello", "My plugin greeting", cmd_hello},
};

plugin_api_t MY_PLUGIN = {
    .init   = my_load,
    .deinit = my_unload,
    .commands     = s_cmds,
    .command_count = 1,
};
```

Then add a `module_t` entry in `kernel/module.c`.

### Event System

| Event | When fired |
|---|---|
| `BOOT_COMPLETE` | After kernel_init() completes |
| `TICK` | Roughly once per second |
| `PRE_COMMAND` | Before a CLI command executes |
| `POST_COMMAND` | After a CLI command executes |
| `PEER_DISCOVERED` | When a swarm mesh peer is found |
| `CUSTOM_START` | Base for user-defined events (100+) |

### Dynamic Command Registration

Loaded modules inject commands through `commands_api_register()`/`commands_api_unregister()`, which maintain a dynamic command table (32 slots). This allows modules to add and remove commands at runtime without recompiling the core firmware.

---

## Commands

Type `help` at the prompt to list all command groups. Use `help <group>` for details on a specific group.

### Core / Info

| Command | Usage | Description |
|---|---|---|
| `help` | `help [group]` | List commands or a specific group |
| `version` | `version` | OS version and build info |
| `clear` | `clear` | Clear the terminal |
| `echo` | `echo <text>` | Print text |
| `uptime` | `uptime` | Time since boot |
| `sysinfo` | `sysinfo` | Full system summary |
| `stats` | `stats` | Runtime statistics |
| `top` | `top` | Live task monitor |
| `uid` | `uid` | Unique device ID (MAC) |

### Hardware

| Command | Usage | Description |
|---|---|---|
| `temp` | `temp` | Internal core temperature |
| `mem` | `mem` | Flash, PSRAM, SD card, camera info |
| `free` | `free` | DRAM and PSRAM heap allocator stats |
| `power` | `power` | VSYS voltage and battery estimate |
| `gpio` | `gpio read\|write\|mode\|irq <pin>` | GPIO operations |
| `led` | `led <on\|off\|toggle\|blink [n]>` | Onboard LED control |
| `pwm` | `pwm <pin> <duty 0-100> [freq_hz]` | PWM output |
| `adc` | `adc <0\|1\|2>` | Read ADC (GPIO26-28) |
| `avg` | `avg <ch> [samples]` | Averaged ADC read |
| `pull` | `pull <pin> <up\|down\|none>` | Set pull resistors |
| `pin` | `pin` | GPIO state snapshot |
| `wdog` | `wdog` | Watchdog status |
| `board` | `board` | ESP32 board info |
| `psram` | `psram` | PSRAM size and free |
| `clock` | `clock` | CPU frequency |

### Buses

| Command | Usage | Description |
|---|---|---|
| `i2c` | `scan [sda scl] \| read \| write \| dump` | I²C bus tools |
| `spi` | `init \| write \| read \| xfer` | SPI bus operations |
| `uart` | `<baud> <tx> <rx> [timeout_s]` | UART passthrough bridge |

### Probes & Analysis

| Command | Usage | Description |
|---|---|---|
| `la` | `<pin> [samples] [us] [trigger]` | Logic analyser |
| `scope` | `<pin> <hz> <ms>` | Waveform viewer |
| `detect` | `scan \| uart <pin> \| analyze <pin>` | Device detection |
| `imu` | `read\|stream\|attitude\|calibrate` | MPU6050 accelerometer/gyro |

### Servo

| Command | Usage | Description |
|---|---|---|
| `servo` | `<pin> <angle 0-180>` | Move servo to angle |
| `servo sweep` | `<pin> [from to step_ms]` | Blocking sweep |
| `servo bg` | `<pin> sweep\|goto\|stop` | Background servo control |

### Audio & Signalling

| Command | Usage | Description |
|---|---|---|
| `tone` | `<pin> <note\|Hz> [ms]` | Play tone on passive buzzer |
| `melody` | `<pin> <notes...> \| elise \| canon` | Play melody sequence |
| `morse` | `<text> [wpm]` | Blink LED in morse |
| `piano` | `<pin>` | Interactive keyboard piano |

### Scripting & Automation

| Command | Usage | Description |
|---|---|---|
| `sleep` | `<ms>` | Wait for milliseconds |
| `repeat` | `<n> <command>` | Run command n times |
| `watch` | `<ms> <command>` | Run command at interval |
| `trigger` | `<pin> <rise\|fall\|both> <cmd>` | Edge-triggered command |
| `cron` | `<delay_ms> <command>` | Deferred command execution |
| `bench` | `<iters> <command>` | Throughput test |
| `time` | `<command>` | Measure execution time |
| `alias` | `[name [cmd...]]` | Define or list aliases |
| `unalias` | `<name>` | Remove an alias |
| `script` | `script <run\|list>` | DeckScript scripting |
| `run` | `run <file>` | Execute a DeckScript file |

### Editor

| Command | Usage | Description |
|---|---|---|
| `edit` | `edit <file>` | Nano-style text editor |

### System

| Command | Usage | Description |
|---|---|---|---|
| `reboot` | `reboot` | Reboot via watchdog |
| `drivers` | `drivers` | List loaded drivers |
| `tasks` | `[enable\|disable <id>]` | Background task control |
| `config` | `show\|set <key> <val>\|save\|reset` | Persistent config |
| `syslog` | `show [n]\|warn\|err\|write\|clear\|stats` | System log |
| `jobs` | `list\|cancel <id>` | Background job control |
| `date` | `[set Y M D h m s]` | Real-time clock |
| `history` | `[clear]` | Command history |
| `uname` | `[-a]` | System identity |
| `rand` | `[min] [max]` | Hardware random number |
| `module` | `module <load\|unload\|list>` | Module management |
| `stack` | `stack` | Task stack high-water mark |
| `fault` | `fault` | Panic info |
| `dashboard` | `start\|stop\|status` | Web dashboard server |
| `ota` | `ota <url>` | OTA firmware update |

### Filesystem

| Command | Usage | Description |
|---|---|---|
| `ls` | `[path]` | List directory |
| `cat` | `<file> [file2 ...]` | Print file contents |
| `touch` | `<file>` | Create or update file |
| `mkdir` | `<dir>` | Create directory |
| `rm` | `[-r] <path>` | Remove file or directory |
| `write` | `<file> <text> \| -i <file>` | Write file |
| `iwrite` | `<file>` | Interactive multi-line write |
| `append` | `<file> <text>` | Append to file |
| `hexdump` | `<file>` | Hex + ASCII dump |
| `cd` | `[dir]` | Change directory |
| `pwd` | `pwd` | Print working directory |
| `cp` | `<src> <dst>` | Copy file |
| `mv` | `<src> <dst>` | Move or rename |
| `stat` | `<path>` | File metadata |
| `wc` | `<file>` | Count lines, words, bytes |
| `grep` | `<pattern> <file>` | Search file |
| `find` | `[name]` | Recursive search |
| `df` | `df` | Filesystem usage |
| `tree` | `tree` | Directory tree |
| `save` | `save` | Persist VFS to SPIFFS |
| `flash` | `flash <read\|write\|erase>` | Raw flash access |

### WiFi

| Command | Usage | Description |
|---|---|---|
| `wifi init` | `wifi init` | Initialise WiFi subsystem |
| `wifi ap` | `ap <ssid> [pass]` | Start softAP mode |
| `wifi scan` | `scan` | Scan for networks |
| `wifi join` | `join <ssid> <pass>` | Connect to network |
| `wifi disconnect` | `disconnect` | Disconnect |
| `wifi status` | `status` | Show connection state |
| `wifi get` | `get <url>` | HTTP GET request |
| `wifi post` | `post <url> <body>` | HTTP POST request |

### Swarm / ESP-NOW

| Command | Usage | Description |
|---|---|---|
| `swarm init` | `init` | Start ESP-NOW mesh |
| `swarm id` | `id <name>` | Set node name |
| `swarm mac` | `mac` | Show MAC address |
| `swarm peer` | `peer <MAC>` | Register a peer |
| `swarm pub` | `pub <lat> <lon> <alt> <hdg> <state>` | Broadcast telemetry |
| `swarm list` | `list` | Show known peers |
| `swarm stop` | `stop` | Stop mesh |

### Camera

| Command | Usage | Description |
|---|---|---|
| `camera init` | `init` | Initialise camera |
| `camera capture` | `capture` | Capture and show JPEG info |
| `camera save` | `save [path]` | Save capture to SPIFFS |
| `camera ls` | `ls` | List saved captures |
| `camera info` | `info` | Camera capabilities |
| `camera res` | `res <QQVGA\|QVGA\|VGA\|...>` | Set resolution |
| `camera quality` | `quality <10-63>` | Set JPEG quality |
| `camera stream` | `stream` | Toggle MJPEG stream on port 81 |

### NRF24L01

| Command | Usage | Description |
|---|---|---|
| `nrf24 init` | `init` | Initialise NRF24L01+ on SPI |
| `nrf24 send` | `send <hex bytes>` | Transmit data |
| `nrf24 listen` | `listen` | Enter receive mode |
| `nrf24 status` | `status` | Show register dump |

### Bluetooth (stubbed)

| Command | Usage | Description |
|---|---|---|
| `bt init` | `init` | Initialise BT (SPP) |
| `bt deinit` | `deinit` | Power down BT |
| `bt status` | `status` | Show connection state |
| `bt shell` | `shell` | Interactive BT shell |

> Bluetooth is disabled in `sdkconfig.defaults` to save DRAM. Remove the `# CONFIG_BT_ENABLED is not set` lines to enable it.

---

## Shell

The shell provides an interactive terminal with:

- **128-byte input buffer** with line editing
- **8-entry command history** with arrow key navigation
- **Aliases** — define custom short names for commands (up to 16)
- **Ctrl-C** — cancel current input
- **Ctrl-D** — shortcut for `uptime`
- **Ctrl-L** — clear screen

### Keyboard shortcuts

| Key | Action |
|---|---|
| `↑` / `↓` | Browse history |
| `Backspace` | Delete character |
| `Ctrl-C` | Cancel input |
| `Ctrl-D` | Quick uptime |
| `Ctrl-L` | Clear screen |

---

## Filesystem (VFS)

DeckOS implements an **in-memory virtual filesystem** (VFS) stored in RAM. It provides a hierarchical directory structure with up to 32 nodes, 512 bytes per file, and paths up to 128 characters.

The VFS is persisted to a **SPIFFS partition** (2 MB) and automatically restored on boot. Any change (write, touch, mkdir, rm, cp, mv) triggers an automatic save.

### VFS layout

```
/
├── home/         (user scripts and data)
│   └── tests/    (built-in DeckScript test suite)
├── tmp/          (temporary files)
└── (user files)
```

### File persistence

The `file_persist` module provides lightweight key-value file persistence under `/persist/` on SPIFFS, used for scripts and data that need to survive reboots independently of the full VFS save/restore.

---

## DeckScript

DeckScript is the built-in scripting language. Scripts are plain text files (`.ds` extension by convention) stored in the VFS and executed with `run <file>` or `script run <file>`.

### Features

- **Variables** — `let name = value`, reference with `$name`
- **Arithmetic** — `+`, `-`, `*`, `/`, `%` in `let` expressions (chained: `1 + 2 + 3`)
- **Conditions** — `==`, `!=`, `<`, `>`, `<=`, `>=` with `&&` / `||` chaining
- **String functions** — `upper()`, `lower()`, `len()`, `substr()`, `contains()`, `trim()`, `replace()`, `format()`
- **Math functions** — `sqrt()`, `pow()`, `abs()`, `min()`, `max()`, `clamp()`, `map()`, `rand()`, `avg()`
- **Control flow** — `if`/`elif`/`else`, `switch`/`case`/`default`
- **Loops** — `repeat`, `while`, `for` (range and array iteration)
- **Arrays** — `arr_new`, `arr_push`, `arr_pop`, `arr_get`, `arr_set`, `arr_len`, `arr_dump`, `for...in`
- **Functions** — `def`/`enddef` with `call`, `$arg0`-`$arg9`, `$return`, recursion
- **Hardware access** — `adc(ch)`, `gpio(pin)`, `pwm(pin, duty)`, `millis`, `micros`
- **Shell commands** — any shell command can be embedded directly in scripts
- **I/O** — `print`, `println`, `input()`, `pause`, `vars`
- **Logging** — `log info/warn/err/debug`
- **Includes** — `include <file>` (shared context), `run <file>` (fresh context)
- **Comments** — lines starting with `#`
- **Assertions** — `assert <condition> or fail: message`

### Example

```
# blink.ds — blink onboard LED 10 times
repeat 10
  gpio_write 33 1
  sleep 200
  gpio_write 33 0
  sleep 200
endrepeat
print done
```

### Limits

| Limit | Value |
|---|---|
| Max lines per script | 128 |
| Max call depth | 4 |
| Max variables | 32 |
| Variable name length | 15 chars |
| Variable value length | 64 chars |
| Line buffer | 512 chars |
| Repeat cap | 10,000 iterations |
| While cap | 100,000 iterations |


---

## WiFi

Native ESP32 WiFi via ESP-IDF. Supports both **station** and **softAP** modes.

### Station mode

```bash
> wifi init
> wifi status
> wifi scan
> wifi join MyNetwork MyPassword
> wifi status
  state: connected
  IP: 192.168.1.42
  SSID: MyNetwork
  RSSI: -45 dBm
> wifi get http://example.com/data
> wifi post http://example.com/log "temp=27.3"
> wifi disconnect
```

### SoftAP mode

```bash
> wifi ap DeckOS_AP mypass
> wifi status
```

With AP + STA simultaneous mode, the ESP32 can act as a WiFi access point while also connecting to a router.

### HTTP client

Raw socket-based HTTP GET and POST with response body extraction (skips HTTP headers). URLs are parsed for host, port, and path.

---

## ESP-NOW Swarm

ESP-NOW is a connectionless peer-to-peer protocol that lets ESP32 nodes communicate directly without a WiFi router.

```bash
# Node 1
> swarm init
> swarm mac          # note MAC: AA:BB:CC:DD:EE:01
> swarm id drone1

# Node 2
> swarm init
> swarm mac          # note MAC: AA:BB:CC:DD:EE:02
> swarm id drone2
> swarm peer AA:BB:CC:DD:EE:01

# Broadcast from node 1
> swarm pub 28.6139 77.2090 100.0 180.0 1
```

### Features

- Up to **20 peers** in the peer table
- **Telemetry broadcast** — lat, lon, alt, heading, state
- **Automatic peer discovery** — received telemetry auto-adds peers
- **Broadcast mode** — packets sent to MAC `FF:FF:FF:FF:FF:FF`

---

## Camera (ESP32-CAM)

On ESP32-CAM boards, the camera subsystem provides OV2640 image capture and MJPEG streaming.

### Pin mapping (ESP32-CAM default)

| Signal | GPIO |
|--------|------|
| XCLK | 4 |
| SIOC (I²C SCL) | 25 |
| SIOD (I²C SDA) | 26 |
| PWDN | 32 |
| RESET | -1 (not used) |
| VSYNC | 27 |
| HREF | 23 |
| PCLK | 22 |
| D0-D7 | 5, 18, 19, 21, 36, 39, 34, 35 |

Or you can just buy an ESP32-CAM programmer, which cuts all these connection efforts.

### Usage

```bash
> camera init
> camera info
  sensor: OV2640
  resolution: 1600x1200
> camera capture
  captured 128340 bytes (JPEG)
> camera save /cap001.jpg
  saved to SPIFFS: /cap001.jpg (128340 B)
> camera ls
  /cap001.jpg   (128340 B)
> camera stream           # MJPEG stream on http://<ip>:81
```

### Resolutions

| Name | Size |
|---|---|
| QQVGA | 160×120 |
| QVGA | 320×240 |
| VGA | 640×480 |
| SVGA | 800×600 |
| XGA | 1024×768 |
| SXGA | 1280×1024 |
| UXGA | 1600×1200 |

---

## Web Dashboard

The built-in web dashboard serves a browser-based UI at **`http://<esp32-ip>/`** for monitoring and controlling the board without a serial connection.

```bash
> wifi init
> wifi join MyWiFi MyPassword
> dashboard start
dashboard: http://192.168.1.42/
> dashboard status
dashboard: running
> dashboard stop
```

### Dashboard features

| Feature | API endpoint | What you can do |
|---------|-------------|-----------------|
| **System info** | `/api/sysinfo` | Uptime, CPU, free heap, WiFi state, IP address |
| **Memory** | `/api/mem` | Total/used/free heap with live bar graph |
| **File browser** | `/api/ls`, `/api/cat`, `/api/write`, `/api/rm` | Browse, view, upload, and delete VFS files |
| **GPIO control** | `/api/gpio?pin=N&val=0\|1` | Read/write GPIO pins from the browser |
| **Script execution** | `/api/script` (POST) | Run any DeckScript command |
| **Reboot** | `/api/reboot` | Remote restart |

The page auto-refreshes every 5 seconds. All data is served as JSON REST APIs for easy integration with external tools.

---

## ESP-NOW RC Bridge

PPM RC commands can be relayed across ESP-NOW from one ESP32 to another. This lets you place a receiver ESP32 near the RC transmitter and control a remote DeckOS board wirelessly.

### Architecture

```
[RC Receiver] → PPM → [ESP32 Bridge] ──ESP-NOW──→ [ESP32 DeckOS]
  (2.4 GHz)          (swarm node A)              (swarm node B)
                      reads PPM ch 0-3           rc commands fire locally
                      broadcasts via ESP-NOW     or forward to DeckScript
```

### Setup

```bash
# Bridge node (connected to RC receiver's PPM output)
> wifi init
> swarm init
> swarm id bridge
> ppm init 16 4           # 4 channels from RC receiver on GP16
> swarm pub once          # broadcast PPM data via ESP-NOW
> swarm pub every 20      # broadcast every 20 ms (~50 Hz)

# Remote DeckOS node
> swarm init
> swarm id deckos
> swarm peer <bridge_mac>
> rc set throttle high 1700
> rc set rudder high 1600
> rc enable
```

The bridge reads PPM channels as DeckOS `ppm` values (1000-2000 µs), packs them into the ESP-NOW telemetry payload, and the receiving node extracts them into local variables accessible by the RC command system or DeckScript.

---

## NRF24L01 Radio

SPI-based 2.4 GHz radio driver supporting the NRF24L01+ module.

```bash
> nrf24 init
  nRF24L01+ initialised on SPI
> nrf24 status
  (full register dump with config, addresses, FIFO state, carrier detect)
```

### Features

- 6 data pipes
- Auto-acknowledgment
- Dynamic payload length
- Configurable RF power (min/low/high/max)
- Configurable datarate (250 kbps, 1 Mbps, 2 Mbps)
- Channel scanner (carrier detect per channel)
- 32-byte max payload

---

## Bluetooth

The ESP32 port includes Bluetooth stubs only. All `bt_*` functions return false or no-op. Bluetooth is disabled in `sdkconfig.defaults` to conserve DRAM (the classic BT controller consumes ~200 KB).

To enable Bluetooth, remove or comment out these lines in `sdkconfig.defaults`:

```
# CONFIG_BT_ENABLED is not set
# CONFIG_BTDM_CONTROLLER_MODE_BTDM is not set
# CONFIG_BT_CLASSIC_ENABLED is not set
# CONFIG_BT_SPP_ENABLED is not set
```

Then rebuild.

---

## OLED (SSD1306)

SSD1306 128×64 monochrome OLED display driver over I²C (address 0x3C). Default pins: GP21 (SDA), GP22 (SCL) on ESP32-WROOM boards.

### Commands

| Command | Description |
|---|---|
| `oled init` | Initialise display |
| `oled on / off` | Power control |
| `oled clear` | Blank display |
| `oled fill <hex>` | Fill with pattern |
| `oled contrast <0-255>` | Set brightness |
| `oled invert <0\|1>` | Invert colours |
| `oled text <col> <row> <str>` | Draw text |
| `oled pixel <x> <y> <0\|1>` | Set pixel |
| `oled line <x0> <y0> <x1> <y1>` | Draw line |
| `oled rect <x> <y> <w> <h>` | Rectangle |
| `oled circle <cx> <cy> <r>` | Circle |
| `oled progress <x> <y> <w> <h> <%>` | Progress bar |
| `oled splash <line1> <line2>` | Splash screen |
| `oled boot` | Animated boot sequence |
| `oled scroll <dir> <sp> <ep>` | Hardware scroll |
| `oled spinner <x> <y> <frame>` | Spinner glyph |

### OLED console mirror

```bash
> console oled on
```

Mirrors shell output to the OLED (8 rows × 21 chars). Useful for standalone/headless operation with a battery.

---

## Servo

Controls standard RC servos via 50 Hz PWM (500-2500 µs pulse, 0-180°).

```bash
> servo 16 90          # centre servo on GP16
> servo sweep 16 0 180 20   # sweep 0°->180°, 20 ms steps
> servo bg 16 sweep 0 180 1 15   # background sweep on Core 1
> servo bg 16 goto 45         # move to 45° in background
> servo bg list               # list active background servos
> servo bg 16 stop            # stop background servo
```

Up to **8 servos** can be controlled simultaneously. Background sweeps run on the Core 1 scheduler task.

---

## Audio

Drives a passive buzzer via PWM (LEDC peripheral).

```bash
> tone 16 C4 500       # play middle C for 500 ms
> tone 16 440 1000     # play 440 Hz for 1 s
> melody 16 C4:200 E4:200 G4:400  # melody sequence
> melody 16 elise      # pre-built Fur Elise
> melody 16 canon      # pre-built Canon in D
> piano 16             # interactive keyboard
```

12 semitones × 8 octaves (C0-C7). Use standard scientific pitch notation: `C4`, `A#3`, `Bb4`, `REST`.

---

## IMU (MPU6050)

MPU6050 6-axis accelerometer/gyroscope on I²C.

```bash
> imu whoami           # check device ID
> imu read             # raw accel + gyro values
> imu stream           # live streaming (Ctrl-C to stop)
> imu attitude         # roll/pitch via complementary filter
> imu calibrate        # gyro offset calibration
```

---

## Modules

See the [Module System](#module-system) section above for the full list of available modules, usage, and the plugin API.

### Text Editor (`edit`)

The editor module supports: arrow keys, Home/End, PgUp/PgDn, Ctrl-S (save), Ctrl-Q/X (quit), Ctrl-K (cut line), Ctrl-Y (paste), Ctrl-U (insert line), Ctrl-F (find), Ctrl-Z (undo), Ctrl-G (help).

---

## Config system

Persistent settings stored in NVS (Non-Volatile Storage on the `nvs` partition).

```bash
> config show
> config set hostname my-esp32
> config show
> config set cpu_mhz 240
> config save
> config reset
```

### Config keys

| Key | Values | Effect |
|---|---|---|
| `hostname` | any string | Name shown at boot |
| `boot_led` | 0 or 1 | LED on at boot |

---

## Syslog

64-entry ring buffer with coloured output.

```bash
> syslog show          # all entries
> syslog show 20       # last 20
> syslog warn          # WARN and above
> syslog err           # errors only
> syslog clear
> syslog stats
```

Levels: `DBG` (grey) -> `INF` (white) -> `WRN` (yellow) -> `ERR` (red).

---

## Scheduler

Cooperative background task scheduler on **Core 1**. Up to 16 tasks with configurable intervals.

```bash
> tasks
ID  ENABLED  INTERVAL  CPU%    NAME
 0   yes      1000 ms   12.3%  servo_bg
 1   yes       500 ms   0.1%   wifi_scan

> tasks disable 0
> tasks enable 0
> top                 # live CPU usage per task
```

CPU usage is tracked per-task in microseconds. Background jobs (servo sweeps, logic analyser triggers) also run on Core 1.

---

## Drivers

Drivers are registered and initialised at boot. Each reports OK or FAIL.

| Driver | What it covers |
|---|---|
| `adc` | ADC1, 12-bit, 0-3.3V |
| `gpio` | GPIO init (ESP-IDF driver) |
| `pwm` | LEDC PWM, 13-bit |
| `i2c0` | I²C master at 100 kHz (default GP21/GP22) |
| `spi0` | SPI2 on default pins (GP18/GP23/GP19) |

```bash
> drivers
```

---

## Hardware Abstraction Layer (HAL)

The HAL wraps ESP-IDF functionality behind a unified API so the kernel and upper layers are platform-agnostic.

| Module | Functions |
|---|---|
| `hal_board` | Board detection (WROOM/CAM/S3), flash/PSRAM size |
| `hal_gpio` | init, direction, get/put, pull, function select |
| `hal_adc` | 12-bit ADC1, channel select, voltage read |
| `hal_i2c` | Master with timeout, scan, write/read, combined write+read |
| `hal_spi` | SPI2 master, DMA, 4096 B/transfer, software CS |
| `hal_pwm` | LEDC PWM, variable duty and frequency |
| `hal_uart` | Interrupt-driven UART with RX callback |
| `hal_flash` | NVS key-value store + SPIFFS mount/read/write/delete/list |
| `hal_timer` | Microsecond/millisecond time, microsleep with watchdog feed |
| `hal_system` | Reboot, watchdog, IRQ disable/restore |
| `hal_console` | UART console init, getchar, putchar, connection check |
| `hal_camera` | ESP32-CAM OV2640: init, capture, resolution, MJPEG stream |

The HAL header (`main/hal/hal.h`) is 126 lines and defines the complete platform abstraction contract.

---

## Board support

| Board | Auto-detected | LED pin | I²C pins | SPI pins | Camera |
|---|---|---|---|---|---|
| ESP32-WROOM | Yes | 33 | 21/22 | 18/23/19 | No |
| ESP32-CAM | Yes | 33 | 25/26 | 14/13/12 | Yes |
| ESP32-S3 | Yes | 33 | 8/9 | 10/11/12 | No |

The board detector uses eFuse features and PSRAM presence to identify the board. Board-specific pin mappings are applied automatically.

---

## Partition layout

```
# Name,    Type, SubType, Offset,  Size
nvs,       data, nvs,     0x9000,  0x6000    (24 KB)
phy_init,  data, phy,     0xf000,  0x1000    (4 KB)
factory,   app,  factory, 0x10000, 0x1F0000  (1984 KB)
spiffs,    data, spiffs,  0x200000,0x200000  (2048 KB)
```

- **nvs**: WiFi calibration, config keys
- **phy_init**: WiFi PHY calibration data
- **factory**: Application code
- **spiffs**: VFS file persistence (2 MB)

---

## Project layout

```
DeckOS_ESP32/
├── CMakeLists.txt                # Top-level IDF project
├── partitions.csv                # Custom partition table
├── sdkconfig.defaults            # Build defaults
├── dependencies.lock             # Component versions
├── main/
│   ├── main.c                    # Entry point
│   ├── CMakeLists.txt            # Component build
│   ├── hal/                      # Hardware Abstraction Layer
│   │   ├── hal.h                 # HAL API header
│   │   ├── hal_gpio.c
│   │   ├── hal_adc.c
│   │   ├── hal_i2c.c
│   │   ├── hal_spi.c
│   │   ├── hal_pwm.c
│   │   ├── hal_uart.c
│   │   ├── hal_timer.c
│   │   ├── hal_flash.c
│   │   ├── hal_board.c
│   │   └── hal_system.c
│   ├── kernel/                   # Core kernel
│   │   ├── kernel.c
│   │   ├── bootloader.c
│   │   ├── scheduler.c
│   │   ├── syslog.c
│   │   ├── config.c
│   │   ├── vfs.c
│   │   ├── file_persist.c
│   │   ├── module.c
│   │   ├── bg_job.c
│   │   ├── gpio_mon.c
│   │   ├── morse.c
│   │   └── tone.c
│   ├── shell/                    # Shell + editor
│   │   ├── shell.c
│   │   └── editor.c
│   ├── commands/                 # Command dispatch
│   │   ├── commands.c            # ~90+ commands
│   │   └── cmd_imu.c
│   ├── scripting/                # DeckScript interpreter
│   │   ├── dscript_core.c        # Parser, run_lines, script_run_*
│   │   ├── dscript_vars.c        # Variable management
│   │   ├── dscript_expr.c        # Condition & arithmetic evaluators
│   │   ├── dscript_builtin.c     # Built-in functions
│   │   ├── dscript_flow.c        # Block matching & array helpers
│   │   └── dscript_internal.h    # Internal API header
│   ├── communication/            # Networking
│   │   ├── wifi.c                # Native WiFi
│   │   ├── swarm.c               # ESP-NOW mesh
│   │   ├── bt_stubs.c            # Bluetooth stubs
│   │   ├── uart_pass.c           # UART passthrough
│   │   └── uart_detect.c         # UART device detection
│   ├── drivers/                  # Hardware drivers
│   │   ├── drivers.c             # Driver registry
│   │   ├── spi_bus.c             # SPI bus abstraction
│   │   ├── oled.c                # SSD1306 display
│   │   ├── mpu6050.c             # IMU
│   │   ├── nrf24l01.c             # NRF24L01 radio
│   │   └── camera/               # ESP32-CAM
│   │       ├── camera_esp32.c
│   │       └── camera_esp32.h
│   ├── hardware/                 # Hardware control
│   │   └── servo.c
│   ├── system/                   # System utilities
│   │   ├── board_detect.c
│   │   ├── device_detect.c
│   │   ├── bench.c
│   │   ├── heap_track.c
│   │   ├── oled_console.c
│   │   └── print_lock.c
│   └── include/                  # Shared headers
│       ├── kernel.h
│       ├── shell.h
│       ├── commands.h
│       ├── vfs.h
│       ├── scheduler.h
│       ├── syslog.h
│       ├── config.h
│       ├── module.h
│       ├── drivers.h
│       ├── spi_bus.h
│       ├── oled.h
│       ├── mpu6050.h
│       ├── nrf24l01.h
│       ├── servo.h
│       ├── wifi.h
│       ├── swarm.h
│       ├── bt.h
│       ├── board_detect.h
│       ├── device_detect.h
│       ├── uart_pass.h
│       ├── uart_detect.h
│       ├── bench.h
│       ├── heap_track.h
│       ├── oled_console.h
│       ├── print_lock.h
│       ├── gpio_mon.h
│       ├── bg_job.h
│       ├── file_persist.h
│       ├── editor.h
│       ├── dscript.h
│       ├── morse.h
│       ├── tone.h
│       ├── bootloader.h
│       └── led.h
├── managed_components/           # IDF component registry
│   └── espressif__esp32-camera/  # esp32-camera v2.0.12
└── build/                        # Build artifacts
```

---

## Original RP2040 DeckOS

The original DeckOS for RP2040 is at [github.com/enginestein/DeckOS](https://github.com/enginestein/DeckOS). It features:

---

---

## Demo

https://github.com/user-attachments/assets/29c5ad36-af89-499c-b757-9b31cec9b44c

---


*Who doesn't love a decent shell?*