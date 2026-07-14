# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

# Aggregate ESP-IDF "linux" host-test project: builds EVERY component's shared
# TEST_CASE sources natively through ESP-IDF (its own Unity) into a single binary,
# so the components are exercised exactly as the firmware compiles them. Runs as a
# plain native executable.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS main)
# FreeRTOS mock (no scheduler — our runner provides main()) + the firmware components.
list(APPEND EXTRA_COMPONENT_DIRS
    "$ENV{IDF_PATH}/tools/mocks/freertos/"
    "${CMAKE_CURRENT_LIST_DIR}/../components")

project(vbankey_host_test)
