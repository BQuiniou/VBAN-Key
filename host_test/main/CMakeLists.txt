# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

# Reuse the SAME shared TEST_CASE sources as the standalone host build and the
# on-target unit-test-app; only the runner entry (test_main.c) is local.
# WHOLE_ARCHIVE keeps the TEST_CASE constructors from being stripped. (No
# CONFIGURE_DEPENDS: ESP-IDF evaluates component CMakeLists in script mode, where
# it is illegal; `make host-idf` reconfigures each run anyway.)
file(GLOB TEST_SRCS ${CMAKE_CURRENT_LIST_DIR}/../../components/*/test/*.c)

idf_component_register(
    SRCS "test_main.c" ${TEST_SRCS}
    INCLUDE_DIRS "."
    REQUIRES unity vban config button runtime vban_net vban_script
    WHOLE_ARCHIVE)
