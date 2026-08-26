# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

idf_component_register(
    SRCS
        "main.c"
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
    PRIV_REQUIRES
        esp_driver_gpio
)

# The platform shim will add ESP-IDF deps as it is implemented, e.g.:
#   PRIV_REQUIRES driver esp_wifi esp_netif nvs_flash esp_timer lwip esp_littlefs
