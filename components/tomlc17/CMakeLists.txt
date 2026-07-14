# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(SRCS "tomlc17.c" INCLUDE_DIRS ".")

# Vendored upstream, kept byte-for-byte: its snprintf() diagnostics use %x/%d on
# uint32_t, which is 'long unsigned int' on the ESP32 target and so trips
# -Werror=format (the host build, where uint32_t is 'unsigned int', is unaffected).
# The printed values are correct; demote format checks to warnings for this
# component only rather than patch third-party code.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format)
