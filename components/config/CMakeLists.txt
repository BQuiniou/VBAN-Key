# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS "src/config.c"
    INCLUDE_DIRS "include"
    REQUIRES tomlc17 common
)

# tomlc17.h uses extensions that strict -std=c99 -pedantic rejects.
set_target_properties(${COMPONENT_LIB} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)
