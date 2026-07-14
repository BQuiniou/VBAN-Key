// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_BUTTON_H
#define VBANKEY_BUTTON_H

#include "common/bitset.h"
#include "common/button_mode.h"

#include <stdbool.h>
#include <stdint.h>

#define BUTTON_DEBOUNCE_MS 20

enum button_event
{
    BUTTON_NONE,
    BUTTON_ENTER_ON,
    BUTTON_ENTER_OFF
};

enum button_flag
{
    BTN_FLAG_RAW,
    BTN_FLAG_STABLE,
    BTN_FLAG_ON
};

struct button
{
    enum button_mode mode;
    uint8_t flags;
    uint32_t changed_ms;
};

void button_init(struct button* b, enum button_mode mode);
enum button_event button_update(struct button* b, bool pressed, uint32_t now_ms);

#endif // VBANKEY_BUTTON_H
