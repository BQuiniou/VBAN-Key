// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_time.h"

#include "esp_timer.h"

uint32_t plat_time_millis(void)
{
    uint64_t const microseconds = (uint64_t) esp_timer_get_time();

    return (uint32_t) (microseconds / 1000U);
}
