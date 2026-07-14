# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS "src/button.c"
    INCLUDE_DIRS "include"
    REQUIRES common
)

# Pure C99 core: strict ISO C99, no GNU extensions.
set_target_properties(${COMPONENT_LIB} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
