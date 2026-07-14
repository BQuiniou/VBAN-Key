// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_SIM_KBD_H
#define VBANKEY_SIM_KBD_H

#include <stdbool.h>
#include <stddef.h>

enum sim_kbd_action
{
    SIM_KBD_NONE,
    SIM_KBD_DOWN,
    SIM_KBD_UP,
    SIM_KBD_TAP,
    SIM_KBD_QUIT
};

struct sim_kbd_event
{
    enum sim_kbd_action action;
    int key;
};

void sim_kbd_init(void);
bool sim_kbd_real_events(void);
bool sim_kbd_poll(struct sim_kbd_event* out);
void sim_kbd_shutdown(void);

/* seq is the CSI-u body after ESC [, including the final 'u'. */
bool sim_kbd_parse_csi_u(char const* seq, size_t len, int* keycode, int* modifiers, int* event_type);

#endif // VBANKEY_SIM_KBD_H
