# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vbankey)

# Build & flash the LittleFS config image from data/ (requires joltwallet/littlefs):
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/device/config.toml")
    message(FATAL_ERROR
        "Missing config/device/config.toml; copy config/examples/config.toml and customize it"
    )
endif()
littlefs_create_partition_image(storage config/device FLASH_IN_PROJECT)
