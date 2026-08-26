// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_GPIO_H
#define PLAT_GPIO_H

#include <stdbool.h>

// ESP-IDF GPIO shim: configure button inputs and status outputs.
void plat_gpio_init(void);

/**
 * Configure one active-low button input with its internal pull-up enabled.
 *
 * Return true on success, or false when pin is invalid or configuration fails.
 */
bool plat_gpio_configure_input(int pin);

/**
 * Read a configured active-low button input.
 *
 * Return true when the button connects the GPIO to ground. An invalid pin is
 * reported as not pressed.
 */
bool plat_gpio_read_pressed(int pin);

#endif // PLAT_GPIO_H
