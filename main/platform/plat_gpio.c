// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_gpio.h"

#include "driver/gpio.h"

#include <stdint.h>

void plat_gpio_init(void)
{
    // Inputs are configured after their pin numbers are loaded from the
    // runtime configuration.
}

bool plat_gpio_configure_input(int pin)
{
    gpio_config_t config = {
        .pin_bit_mask = 0U,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (pin < 0 || !GPIO_IS_VALID_GPIO(pin))
    {
        return false;
    }

    config.pin_bit_mask = UINT64_C(1) << (unsigned int) pin;
    return gpio_config(&config) == ESP_OK;
}

bool plat_gpio_read_pressed(int pin)
{
    if (pin < 0 || !GPIO_IS_VALID_GPIO(pin))
    {
        return false;
    }
    return gpio_get_level((gpio_num_t) pin) == 0;
}
