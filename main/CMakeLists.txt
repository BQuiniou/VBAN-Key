# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS
        "main.c"
        "platform/plat_exec.c"
        "platform/plat_gpio.c"
        "platform/plat_net.c"
        "platform/plat_fs.c"
        "platform/plat_time.c"
    INCLUDE_DIRS
        "."
        "platform"
    REQUIRES
        vban
        config
        vban_script
        button
        runtime
        vban_net
    PRIV_REQUIRES
        esp_driver_gpio
        esp_timer
        joltwallet__littlefs
)

# The platform shim will add ESP-IDF deps as it is implemented, e.g.:
#   PRIV_REQUIRES esp_wifi esp_netif nvs_flash lwip
