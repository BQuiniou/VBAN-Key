// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_GPIO_H
#define PLAT_GPIO_H

// ESP-IDF GPIO shim: configure button inputs and status outputs.
// TODO: read/write/interrupt API, defined alongside the button layer.
void plat_gpio_init(void);

#endif // PLAT_GPIO_H
