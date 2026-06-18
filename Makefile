# DeckOS ESP32 — Convenience Makefile
#
# Wraps ESP-IDF's idf.py for common operations.
# Usage:  make <target>  [PORT=/dev/ttyUSB0]  [BAUD=460800]
#

PORT   ?= /dev/ttyACM0
BAUD   ?= 460800
IDF    ?= $(HOME)/embedded/esp/esp-idf
IDF_PY ?= $(IDF)/tools/idf.py

# Detect if we're in an IDF shell environment
IDF_ACTIVE := $(shell echo "$$IDF_PATH" 2>/dev/null)

ifeq ($(IDF_ACTIVE),)
  # Not in an IDF shell — source export.sh before every invocation
  SHIM := bash -c '. $(IDF)/export.sh >/dev/null 2>&1 && exec "$$0" "$$@"'
  SPAWN = $(SHIM) $(IDF_PY)
else
  SPAWN = $(IDF_PY)
endif

.PHONY: all build flash monitor menuconfig clean distclean \
        flash-monitor erase size size-components size-files \
        uf2 reconfigure help

all: build

build:
	$(SPAWN) build

flash:
	$(SPAWN) -p $(PORT) -b $(BAUD) flash

monitor:
	$(SPAWN) -p $(PORT) -b $(BAUD) monitor

flash-monitor: flash monitor

menuconfig:
	$(SPAWN) menuconfig

reconfigure:
	$(SPAWN) reconfigure

clean:
	$(SPAWN) clean

distclean:
	$(SPAWN) fullclean

erase:
	$(SPAWN) -p $(PORT) erase-flash

size:
	$(SPAWN) size

size-components:
	$(SPAWN) size-components

size-files:
	$(SPAWN) size-files

uf2:
	$(SPAWN) uf2

help:
	@echo "DeckOS ESP32 Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  build            Build the project (default)"
	@echo "  flash            Flash to device (PORT=$(PORT))"
	@echo "  monitor          Open serial monitor"
	@echo "  flash-monitor    Flash then monitor"
	@echo "  menuconfig       Open ESP-IDF project configuration"
	@echo "  clean            Remove build artifacts"
	@echo "  distclean        Full clean (remove entire build/)"
	@echo "  erase            Erase entire flash"
	@echo "  size             Show binary sizes"
	@echo "  size-components  Per-component size breakdown"
	@echo "  size-files       Per-file size breakdown"
	@echo "  uf2              Generate UF2 image"
	@echo "  reconfigure      Re-run CMake"
	@echo ""
	@echo "Variables:"
	@echo "  PORT=/dev/ttyACM0   Serial port"
	@echo "  BAUD=460800         Baud rate for flashing"
	@echo "  IDF=path            ESP-IDF directory"
