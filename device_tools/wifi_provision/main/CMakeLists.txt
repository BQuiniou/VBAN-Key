# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS
        "main.c"
        "../../../main/platform/plat_credentials.c"
    INCLUDE_DIRS
        "../../../main/platform"
    PRIV_REQUIRES
        esp_driver_usb_serial_jtag
        nvs_flash
)
