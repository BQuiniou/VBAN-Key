// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "esp_log.h"

#include "plat_fs.h"
#include "plat_gpio.h"
#include "plat_net.h"

static char const* TAG = "vban-key";

void app_main(void)
{
    ESP_LOGI(TAG, "VBAN-Key starting");

    plat_fs_init();
    plat_net_init();
    plat_gpio_init();

    // TODO: load config -> connect Wi-Fi -> run the button loop.
}
