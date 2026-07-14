// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_RUNTIME_H
#define VBANKEY_RUNTIME_H

#include "button/button.h"
#include "config/config.h"
#include "vban_script/vban_script.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct live_button
{
    struct button fsm;
    int gpi_pin;
    struct vs_script init;
    struct vs_script on;
    struct vs_script off;
};

/* This struct is large (tens of KB); it must be static or heap allocated, never stack allocated. */
struct runtime
{
    struct live_button btn[CFG_MAX_BUTTONS];
    size_t n;
    uint8_t midi_arena[VS_MIDI_ARENA];
    struct vs_arena arena;
};

struct runtime_io
{
    void* ctx;
    bool (*read_pressed)(void* ctx, int gpi_pin); /* already active-low normalized */
    uint32_t (*now_ms)(void* ctx);
    void (*fire)(void* ctx, size_t button_index, const struct vs_script* script); /* enqueue; cancels in-flight run */
};

enum runtime_result
{
    RUNTIME_OK = 0,
    RUNTIME_ERR
};

int runtime_build(struct runtime* rt, const struct config* cfg, char* err, size_t err_len);
void runtime_start(struct runtime* rt, const struct runtime_io* io);
void runtime_tick(struct runtime* rt, const struct runtime_io* io);

#endif // VBANKEY_RUNTIME_H
