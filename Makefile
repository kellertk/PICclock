# PICclock Makefile

-include config.mk

ifeq ($(OS),Windows_NT)
	RUNTIME_OS := windows
else
	RUNTIME_OS := unix
endif

BUILD_DIR := build
SRC_DIR := src

XC8 ?= xc8-cc
DFP ?= $(error Run ./configure first)
IPE ?= $(error Run ./configure first)
PROGRAMMER ?= PK5
MCU ?= 16F18344

CFLAGS := -mcpu=$(MCU) -O2 -std=c99
LDFLAGS := -mcpu=$(MCU) -mwarn=-3

FW_SRC := $(wildcard $(SRC_DIR)/*.c)
FW_HEX := $(BUILD_DIR)/PICclock.hex

.PHONY: all clean flash help

all: $(FW_HEX)

$(FW_HEX): $(FW_SRC) | $(BUILD_DIR)
	"$(XC8)" $(CFLAGS) "-mdfp=$(DFP)" $(LDFLAGS) -o $@ $(FW_SRC)

$(BUILD_DIR):
ifeq ($(RUNTIME_OS),windows)
	if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
else
	mkdir -p $(BUILD_DIR)
endif

flash: $(FW_HEX)
	"$(IPE)" -TP$(PROGRAMMER) -P$(MCU) -W -M -F"$(FW_HEX)"

clean:
ifeq ($(RUNTIME_OS),windows)
	if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
else
	rm -rf $(BUILD_DIR)
endif

define HELPTEXT
PICclock

Targets:
  all   - Build firmware (default)
  flash - Flash firmware using MPLAB IPE
  clean - Remove build outputs
  help  - Show this help

Configuration (override with environment variables):
  MCU        = $(MCU)
  PROGRAMMER = $(PROGRAMMER)

Run ./configure first.
endef

help:
	$(info $(HELPTEXT))
