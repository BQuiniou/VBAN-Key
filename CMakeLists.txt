# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(vbankey)

# Build & flash the LittleFS config image from data/ (requires joltwallet/littlefs):
# littlefs_create_partition_image(storage data FLASH_IN_PROJECT)
