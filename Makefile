# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

.DEFAULT_GOAL := help

# The native host build lands in a per-OS-and-arch directory, detected here, so
# checkouts that share one tree (e.g. a macOS host and a Linux VM over a network
# mount, or the same OS on different CPUs) never clobber each other's CMake cache.
# Override HOST_ARCH to cross-build into a distinct tree.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  HOST_OS := macos
else
  HOST_OS := linux
endif
HOST_ARCH := $(shell uname -m)

BUILD_HOST      ?= build/$(HOST_OS)-$(HOST_ARCH)
BUILD_IDF       ?= build/idf-esp32c3
BUILD_IDF_LINUX ?= build/idf-linux
BUILD_GPIO_TEST ?= build/idf-gpio-test
BUILD_WIFI_PROVISION ?= build/idf-wifi-provision
GENERATOR       ?= Ninja
IDF             ?= idf.py
IDF_PYTHON      = $(IDF_PYTHON_ENV_PATH)/bin/python

# Each ESP-IDF target gets its own build dir *and* its own sdkconfig, so the
# firmware (esp32c3) and the Linux host-test target coexist in parallel trees
# and never need a `set-target` flip. The esp32c3 target is read from
# sdkconfig.defaults; the Linux target is selected explicitly (see host-idf).
IDF_C3 = $(IDF) -B $(BUILD_IDF) -D SDKCONFIG=$(BUILD_IDF)/sdkconfig
IDF_GPIO_TEST = $(IDF) -C device_test/gpio -B $(CURDIR)/$(BUILD_GPIO_TEST) \
	-D SDKCONFIG=$(CURDIR)/$(BUILD_GPIO_TEST)/sdkconfig
IDF_WIFI_PROVISION = $(IDF) -C device_tools/wifi_provision \
	-B $(CURDIR)/$(BUILD_WIFI_PROVISION) \
	-D SDKCONFIG=$(CURDIR)/$(BUILD_WIFI_PROVISION)/sdkconfig

.PHONY: help check-idf host test sim demo build flash monitor menuconfig set-target reconfigure host-idf gpio-test-build gpio-test-flash gpio-test-monitor wifi-provision-build wifi-provision-flash wifi-provision clean

help: ## Show available targets
	@grep '^[a-zA-Z_-][a-zA-Z_-]*:.*##' $(MAKEFILE_LIST) | sed 's/:.*## /\t/'

check-idf:
	@test -n "$(IDF_PATH)" -a -n "$(IDF_PYTHON_ENV_PATH)" || { \
	    echo "ESP-IDF environment is not active; source ESP-IDF export.sh first."; \
	    exit 2; \
	}
	@test -x "$(IDF_PYTHON)" || { \
	    echo "ESP-IDF Python environment is unavailable: $(IDF_PYTHON)"; \
	    exit 2; \
	}

host: ## Configure, build, and test the native host targets (build/macos|build/linux by OS)
	cmake -S host -B $(BUILD_HOST) -G "$(GENERATOR)"
	cmake --build $(BUILD_HOST)
	ctest --test-dir $(BUILD_HOST) --output-on-failure

test: host ## Alias for host

sim: ## Configure if needed and build the simulator
	@if [ ! -f "$(BUILD_HOST)/CMakeCache.txt" ]; then cmake -S host -B $(BUILD_HOST) -G "$(GENERATOR)"; fi
	cmake --build $(BUILD_HOST) --target vbankey-sim
	@echo "Run $(BUILD_HOST)/vbankey-sim"

demo: sim ## Build and run the simulator demo
	$(BUILD_HOST)/vbankey-sim --demo

build: check-idf ## Build the ESP-IDF firmware (build/idf-esp32c3)
	$(IDF_C3) build

flash: check-idf ## Flash the firmware and open the monitor
	$(IDF_C3) $(if $(PORT),-p $(PORT),) flash monitor

monitor: check-idf ## Open the ESP-IDF serial monitor
	$(IDF_C3) $(if $(PORT),-p $(PORT),) monitor

menuconfig: check-idf ## Open the ESP-IDF configuration menu
	$(IDF_C3) menuconfig

set-target: check-idf ## (Re)initialise the esp32c3 firmware build dir
	$(IDF_C3) set-target esp32c3

reconfigure: check-idf ## Reconfigure the ESP-IDF firmware project
	$(IDF_C3) reconfigure

host-idf: check-idf ## Build + run all component tests via the ESP-IDF Linux target (build/idf-linux)
	$(IDF) -C host_test -B $(CURDIR)/$(BUILD_IDF_LINUX) \
	    -D SDKCONFIG=$(CURDIR)/$(BUILD_IDF_LINUX)/sdkconfig --preview set-target linux build
	$(CURDIR)/$(BUILD_IDF_LINUX)/vbankey_host_test.elf

gpio-test-build: check-idf ## Build the ESP32-C3 GPI hardware-test application
	$(IDF_GPIO_TEST) build

gpio-test-flash: check-idf ## Flash the GPI hardware-test application
	$(IDF_GPIO_TEST) $(if $(PORT),-p $(PORT),) flash

gpio-test-monitor: check-idf ## Open the GPI hardware-test serial monitor
	$(IDF_GPIO_TEST) $(if $(PORT),-p $(PORT),) monitor

wifi-provision-build: check-idf ## Build the Wi-Fi provisioning firmware
	$(IDF_WIFI_PROVISION) build

wifi-provision-flash: check-idf ## Flash the Wi-Fi provisioning firmware
	$(IDF_WIFI_PROVISION) $(if $(PORT),-p $(PORT),) flash

wifi-provision: check-idf ## Store Wi-Fi credentials (PORT and SSID required)
	@test -n "$(PORT)" || { echo "PORT is required"; exit 2; }
	@test -n "$(SSID)" || { echo "SSID is required"; exit 2; }
	"$(IDF_PYTHON)" tools/provision_wifi.py "$(PORT)" "$(SSID)"

clean: ## Remove all build outputs
	rm -rf build
