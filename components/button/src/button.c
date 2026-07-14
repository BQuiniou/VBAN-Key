// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "button/button.h"

void button_init(struct button* b, enum button_mode mode)
{
    b->mode = mode;
    b->flags = 0U;
    b->changed_ms = 0U;
}

enum button_event button_update(struct button* b, bool pressed, uint32_t now_ms)
{
    bool const raw = bitset_get(b->flags, BTN_FLAG_RAW);
    bool const stable = bitset_get(b->flags, BTN_FLAG_STABLE);

    if (pressed != raw)
    {
        b->flags = (uint8_t) bitset_put(b->flags, BTN_FLAG_RAW, pressed);
        b->changed_ms = now_ms;
        return BUTTON_NONE;
    }
    if (raw != stable && (now_ms - b->changed_ms) >= BUTTON_DEBOUNCE_MS)
    {
        b->flags = (uint8_t) bitset_put(b->flags, BTN_FLAG_STABLE, raw);
        if (raw)
        {
            if (b->mode == BUTTON_MOMENTARY)
            {
                return BUTTON_ENTER_ON;
            }
            b->flags = (uint8_t) bitset_put(b->flags, BTN_FLAG_ON, !bitset_get(b->flags, BTN_FLAG_ON));
            return bitset_get(b->flags, BTN_FLAG_ON) ? BUTTON_ENTER_ON : BUTTON_ENTER_OFF;
        }
        if (b->mode == BUTTON_MOMENTARY)
        {
            return BUTTON_ENTER_OFF;
        }
    }
    return BUTTON_NONE;
}
