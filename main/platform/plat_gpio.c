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

static bool output_pin_allowed(int pin)
{
    return GPIO_IS_VALID_OUTPUT_GPIO(pin) && pin != 2 && pin != 8 && pin != 9 && (pin < 11 || pin > 19);
}

bool plat_gpio_configure_output(int pin, bool level)
{
    gpio_config_t config = {
        .pin_bit_mask = 0U,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (!output_pin_allowed(pin) || gpio_set_level((gpio_num_t) pin, level ? 1U : 0U) != ESP_OK)
    {
        return false;
    }
    config.pin_bit_mask = UINT64_C(1) << (unsigned int) pin;
    return gpio_config(&config) == ESP_OK;
}

void plat_gpio_write(int pin, bool level)
{
    if (output_pin_allowed(pin))
    {
        (void) gpio_set_level((gpio_num_t) pin, level ? 1U : 0U);
    }
}
