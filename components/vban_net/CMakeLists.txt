# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(SRCS "src/vban_net.c" INCLUDE_DIRS "include" REQUIRES vban config lwip)
set_target_properties(${COMPONENT_LIB} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
