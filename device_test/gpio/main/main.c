// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "plat_gpio.h"

#include <stdbool.h>

#define TEST_GPIO_PIN 4
#define TEST_POLL_MS  10

static char const* TAG = "gpio-test";

void app_main(void)
{
    bool previous;

    ESP_LOGI(TAG, "GPI hardware test starting");

    plat_gpio_init();
    if (!plat_gpio_configure_input(TEST_GPIO_PIN))
    {
        ESP_LOGE(TAG, "Failed to configure GPIO%d", TEST_GPIO_PIN);
        return;
    }

    previous = plat_gpio_read_pressed(TEST_GPIO_PIN);
    ESP_LOGI(TAG, "GPIO%d ready: connect a normally-open button between GPIO%d and G", TEST_GPIO_PIN, TEST_GPIO_PIN);
    ESP_LOGI(TAG, "GPIO%d initial state: %s", TEST_GPIO_PIN, previous ? "pressed" : "released");

    for (;;)
    {
        bool const pressed = plat_gpio_read_pressed(TEST_GPIO_PIN);

        if (pressed != previous)
        {
            ESP_LOGI(TAG, "GPIO%d %s", TEST_GPIO_PIN, pressed ? "pressed" : "released");
            previous = pressed;
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_POLL_MS));
    }
}
