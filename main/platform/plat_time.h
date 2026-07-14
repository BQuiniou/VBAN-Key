// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_TIME_H
#define PLAT_TIME_H

#include <stdint.h>

// Milliseconds since boot, for button debounce timing.
uint32_t plat_time_millis(void);

#endif // PLAT_TIME_H
