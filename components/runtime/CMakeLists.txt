# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(SRCS "src/runtime.c" INCLUDE_DIRS "include" REQUIRES config vban_script button common)

# includes config.h (-> tomlc17.h); same rationale as config/vban_script:
set_target_properties(${COMPONENT_LIB} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
