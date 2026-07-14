// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "runtime/runtime.h"

int runtime_build(struct runtime* rt, const struct config* cfg, char* err, size_t err_len)
{
    size_t count;
    size_t index;

    if (rt == NULL || cfg == NULL)
    {
        return RUNTIME_ERR;
    }
    rt->n = 0U;
    rt->arena = (struct vs_arena) {rt->midi_arena, VS_MIDI_ARENA, 0U};
    count = cfg->n_buttons < CFG_MAX_BUTTONS ? cfg->n_buttons : CFG_MAX_BUTTONS;
    for (index = 0U; index < count; ++index)
    {
        const struct cfg_button* source = &cfg->buttons[index];
        struct live_button* button = &rt->btn[index];

        button_init(&button->fsm, source->type);
        button->gpi_pin = source->gpi_pin;
        if (vban_script_compile(source->init, cfg, &rt->arena, &button->init, err, err_len) != VS_OK ||
            vban_script_compile(source->on, cfg, &rt->arena, &button->on, err, err_len) != VS_OK ||
            vban_script_compile(source->off, cfg, &rt->arena, &button->off, err, err_len) != VS_OK)
        {
            return RUNTIME_ERR;
        }
    }
    rt->n = count;
    return RUNTIME_OK;
}

void runtime_start(struct runtime* rt, const struct runtime_io* io)
{
    size_t index;

    if (rt == NULL || io == NULL || io->fire == NULL)
    {
        return;
    }
    for (index = 0U; index < rt->n; ++index)
    {
        io->fire(io->ctx, index, &rt->btn[index].init);
    }
}

void runtime_tick(struct runtime* rt, const struct runtime_io* io)
{
    uint32_t now;
    size_t index;

    if (rt == NULL || io == NULL || io->read_pressed == NULL || io->now_ms == NULL || io->fire == NULL)
    {
        return;
    }
    now = io->now_ms(io->ctx);
    for (index = 0U; index < rt->n; ++index)
    {
        bool const pressed = io->read_pressed(io->ctx, rt->btn[index].gpi_pin);
        const enum button_event event = button_update(&rt->btn[index].fsm, pressed, now);

        if (event == BUTTON_ENTER_ON)
        {
            io->fire(io->ctx, index, &rt->btn[index].on);
        }
        else if (event == BUTTON_ENTER_OFF)
        {
            io->fire(io->ctx, index, &rt->btn[index].off);
        }
    }
}
