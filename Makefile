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
GENERATOR       ?= Ninja
IDF             ?= idf.py

# Each ESP-IDF target gets its own build dir *and* its own sdkconfig, so the
# firmware (esp32c3) and the Linux host-test target coexist in parallel trees
# and never need a `set-target` flip. The esp32c3 target is read from
# sdkconfig.defaults; the Linux target is selected explicitly (see host-idf).
IDF_C3 = $(IDF) -B $(BUILD_IDF) -D SDKCONFIG=$(BUILD_IDF)/sdkconfig

.PHONY: help host test sim demo build flash monitor menuconfig set-target reconfigure host-idf clean

help: ## Show available targets
	@grep '^[a-zA-Z_-][a-zA-Z_-]*:.*##' $(MAKEFILE_LIST) | sed 's/:.*## /\t/'

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

build: ## Build the ESP-IDF firmware (build/idf-esp32c3)
	$(IDF_C3) build

flash: ## Flash the firmware and open the monitor
	$(IDF_C3) flash monitor

monitor: ## Open the ESP-IDF serial monitor
	$(IDF_C3) monitor

menuconfig: ## Open the ESP-IDF configuration menu
	$(IDF_C3) menuconfig

set-target: ## (Re)initialise the esp32c3 firmware build dir
	$(IDF_C3) set-target esp32c3

reconfigure: ## Reconfigure the ESP-IDF firmware project
	$(IDF_C3) reconfigure

host-idf: ## Build + run all component tests via the ESP-IDF Linux target (build/idf-linux)
	$(IDF) -C host_test -B $(CURDIR)/$(BUILD_IDF_LINUX) \
	    -D SDKCONFIG=$(CURDIR)/$(BUILD_IDF_LINUX)/sdkconfig --preview set-target linux build
	$(CURDIR)/$(BUILD_IDF_LINUX)/vbankey_host_test.elf

clean: ## Remove all build outputs
	rm -rf build
