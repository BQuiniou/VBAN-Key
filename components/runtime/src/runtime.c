// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "runtime/runtime.h"

#include <stdio.h>
#include <string.h>

static void drive_gpo(const struct runtime_io* io, const struct cfg_gpo* gpo, bool on)
{
    if (gpo != NULL && io->write_gpo != NULL)
    {
        io->write_gpo(io->ctx, gpo, runtime_gpo_level(gpo, on));
    }
}

bool runtime_gpo_level(const struct cfg_gpo* gpo, bool on)
{
    return on != gpo->active_low;
}

int runtime_build(struct runtime* rt, const struct config* cfg, char* err, size_t err_len)
{
    size_t count;
    size_t index;

    if (rt == NULL || cfg == NULL)
    {
        return RUNTIME_ERR;
    }
    rt->n = 0U;
    rt->cfg = cfg;
    rt->arena = (struct vs_arena) {rt->midi_arena, VS_MIDI_ARENA, 0U};
    count = cfg->n_buttons < CFG_MAX_BUTTONS ? cfg->n_buttons : CFG_MAX_BUTTONS;
    for (index = 0U; index < count; ++index)
    {
        const struct cfg_button* source = &cfg->buttons[index];
        struct live_button* button = &rt->btn[index];

        button_init(&button->fsm, source->type);
        button->gpi_pin = source->gpi_pin;
        button->gpo = config_find_gpo(cfg, source->gpo_id);
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
    for (index = 0U; rt->cfg != NULL && index < rt->cfg->n_gpos; ++index)
    {
        drive_gpo(io, &rt->cfg->gpos[index], false);
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
            drive_gpo(io, rt->btn[index].gpo, true);
            io->fire(io->ctx, index, &rt->btn[index].on);
        }
        else if (event == BUTTON_ENTER_OFF)
        {
            drive_gpo(io, rt->btn[index].gpo, false);
            io->fire(io->ctx, index, &rt->btn[index].off);
        }
    }
}

static bool span_equals(char const* span, size_t length, char const* text)
{
    return length == strlen(text) && memcmp(span, text, length) == 0;
}

int runtime_receive_text(const struct runtime* rt, const struct runtime_io* io, char const* payload, char* err, size_t err_len)
{
    struct vs_assignment assignments[VS_MAX_CMDS];
    size_t count;
    size_t index;

    if (rt == NULL || rt->cfg == NULL || io == NULL)
    {
        return RUNTIME_ERR;
    }
    if (vban_script_parse_payload(payload, assignments, VS_MAX_CMDS, &count, err, err_len) != VS_OK)
    {
        return RUNTIME_ERR;
    }
    for (index = 0U; index < count; ++index)
    {
        const struct vs_assignment* assignment = &assignments[index];
        const struct cfg_gpo* gpo;

        if (!span_equals(assignment->target, assignment->target_len, "GPO") || assignment->field_len != 0U)
        {
            continue;
        }
        gpo = config_find_gpo(rt->cfg, assignment->index);
        if (gpo == NULL || (!span_equals(assignment->value, assignment->value_len, "0") &&
                            !span_equals(assignment->value, assignment->value_len, "1")))
        {
            if (err != NULL && err_len != 0U)
            {
                (void) snprintf(
                    err, err_len, "ignored GPO[%lld] = %.*s", (long long) assignment->index, (int) assignment->value_len,
                    assignment->value
                );
            }
            continue;
        }
        drive_gpo(io, gpo, assignment->value[0] == '1');
    }
    return RUNTIME_OK;
}
