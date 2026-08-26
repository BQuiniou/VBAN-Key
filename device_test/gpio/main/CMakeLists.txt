# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS
        "main.c"
        "../../../main/platform/plat_gpio.c"
    INCLUDE_DIRS
        "../../../main/platform"
    PRIV_REQUIRES
        esp_driver_gpio
)
