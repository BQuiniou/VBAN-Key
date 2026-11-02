// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "runtime/runtime.h"
#include "unity.h"

#include <string.h>

struct fired_item
{
    size_t button_index;
    const struct vs_script* script;
};

struct gpo_write
{
    int pin;
    bool high;
};

struct mock_io
{
    bool pressed;
    uint32_t now;
    struct fired_item fired[3];
    size_t n;
    struct gpo_write writes[8];
    size_t n_writes;
};

static void mock_write_gpo(void* ctx, const struct cfg_gpo* gpo, bool high)
{
    struct mock_io* mock = ctx;

    mock->writes[mock->n_writes].pin = gpo->gpo_pin;
    mock->writes[mock->n_writes].high = high;
    ++mock->n_writes;
}

static void assert_write(const struct mock_io* mock, size_t index, int pin, bool high)
{
    TEST_ASSERT_TRUE(index < mock->n_writes);
    TEST_ASSERT_EQUAL_INT(pin, mock->writes[index].pin);
    TEST_ASSERT_EQUAL(high, mock->writes[index].high);
}

static bool mock_read_pressed(void* ctx, int gpi_pin)
{
    struct mock_io* mock = ctx;

    TEST_ASSERT_EQUAL_INT(4, gpi_pin);
    return mock->pressed;
}

static uint32_t mock_now_ms(void* ctx)
{
    const struct mock_io* mock = ctx;

    return mock->now;
}

static void mock_fire(void* ctx, size_t button_index, const struct vs_script* script)
{
    struct mock_io* mock = ctx;

    mock->fired[mock->n].button_index = button_index;
    mock->fired[mock->n].script = script;
    ++mock->n;
}

static void settle(struct runtime* rt, const struct runtime_io* io, struct mock_io* mock, bool pressed)
{
    mock->pressed = pressed;
    runtime_tick(rt, io);
    mock->now += BUTTON_DEBOUNCE_MS;
    runtime_tick(rt, io);
}

TEST_CASE("build, start, and latch transitions", "[runtime]")
{
    static struct config cfg;
    static struct runtime rt;
    struct mock_io mock = {0};
    const struct runtime_io io = {
        .ctx = &mock,
        .read_pressed = mock_read_pressed,
        .now_ms = mock_now_ms,
        .fire = mock_fire,
    };
    char err[128];

    cfg.text_out[0] = (struct cfg_stream) {.id = 1, .name = "Command1"};
    cfg.n_text_out = 1U;
    cfg.midi_out[0] = (struct cfg_stream) {.id = 1, .name = "MIDI1"};
    cfg.n_midi_out = 1U;
    cfg.buttons[0] = (struct cfg_button) {
        .id = 1,
        .type = BUTTON_LATCH,
        .gpi_pin = 4,
        .init = "SendText(\"vban1\", Strip(0).mute=0;);",
        .on = "SendText(\"vban1\", Strip(0).mute=1;);",
        .off = "SendText(\"vban1\", Strip(0).mute=0;);",
    };
    cfg.n_buttons = 1U;

    TEST_ASSERT_EQUAL(RUNTIME_OK, runtime_build(&rt, &cfg, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(1U, rt.n);
    TEST_ASSERT_EQUAL_UINT(1U, rt.btn[0].on.n);
    TEST_ASSERT_EQUAL(BUTTON_LATCH, rt.btn[0].fsm.mode);

    runtime_start(&rt, &io);
    TEST_ASSERT_EQUAL_UINT(1U, mock.n);
    TEST_ASSERT_EQUAL_UINT(0U, mock.fired[0].button_index);
    TEST_ASSERT_EQUAL_PTR(&rt.btn[0].init, mock.fired[0].script);

    settle(&rt, &io, &mock, true);
    TEST_ASSERT_EQUAL_UINT(2U, mock.n);
    TEST_ASSERT_EQUAL_UINT(0U, mock.fired[1].button_index);
    TEST_ASSERT_EQUAL_PTR(&rt.btn[0].on, mock.fired[1].script);

    settle(&rt, &io, &mock, false);
    TEST_ASSERT_EQUAL_UINT(2U, mock.n);
    settle(&rt, &io, &mock, true);
    TEST_ASSERT_EQUAL_UINT(3U, mock.n);
    TEST_ASSERT_EQUAL_UINT(0U, mock.fired[2].button_index);
    TEST_ASSERT_EQUAL_PTR(&rt.btn[0].off, mock.fired[2].script);
}

static void init_gpo_config(struct config* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->gpos[0] = (struct cfg_gpo) {.id = 1, .gpo_pin = 6, .active_low = false};
    cfg->gpos[1] = (struct cfg_gpo) {.id = 2, .gpo_pin = 7, .active_low = true};
    cfg->n_gpos = 2U;
    cfg->buttons[0] = (struct cfg_button) {.id = 1, .type = BUTTON_LATCH, .gpi_pin = 4, .gpo_id = 1};
    cfg->n_buttons = 1U;
}

TEST_CASE("start drives every gpo off and a button drives its gpo", "[runtime]")
{
    static struct config cfg;
    static struct runtime rt;
    struct mock_io mock = {0};
    const struct runtime_io io = {&mock, mock_read_pressed, mock_now_ms, mock_fire, mock_write_gpo};
    char err[128];

    init_gpo_config(&cfg);
    TEST_ASSERT_EQUAL(RUNTIME_OK, runtime_build(&rt, &cfg, err, sizeof err));
    runtime_start(&rt, &io);
    TEST_ASSERT_EQUAL_UINT(2U, mock.n_writes);
    assert_write(&mock, 0U, 6, false);
    assert_write(&mock, 1U, 7, true);

    settle(&rt, &io, &mock, true);
    TEST_ASSERT_EQUAL_UINT(3U, mock.n_writes);
    assert_write(&mock, 2U, 6, true);
    settle(&rt, &io, &mock, false);
    settle(&rt, &io, &mock, true);
    TEST_ASSERT_EQUAL_UINT(4U, mock.n_writes);
    assert_write(&mock, 3U, 6, false);
}

TEST_CASE("received GPO assignments drive configured gpos only", "[runtime]")
{
    static struct config cfg;
    static struct runtime rt;
    struct mock_io mock = {0};
    const struct runtime_io io = {&mock, mock_read_pressed, mock_now_ms, mock_fire, mock_write_gpo};
    char err[128];

    init_gpo_config(&cfg);
    TEST_ASSERT_EQUAL(RUNTIME_OK, runtime_build(&rt, &cfg, err, sizeof err));
    TEST_ASSERT_EQUAL(
        RUNTIME_OK,
        runtime_receive_text(&rt, &io, "GPO[2] = 1; Strip(0).mute = 1; GPO[9] = 1; GPO[1] = 2; GPO[1] = 1;", err, sizeof err)
    );
    TEST_ASSERT_EQUAL_UINT(2U, mock.n_writes);
    assert_write(&mock, 0U, 7, false);
    assert_write(&mock, 1U, 6, true);
    TEST_ASSERT_EQUAL(RUNTIME_ERR, runtime_receive_text(&rt, &io, "GPO[1 = 1;", err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(2U, mock.n_writes);
}
