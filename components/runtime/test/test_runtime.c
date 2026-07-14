// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "runtime/runtime.h"
#include "unity.h"

struct fired_item
{
    size_t button_index;
    const struct vs_script* script;
};

struct mock_io
{
    bool pressed;
    uint32_t now;
    struct fired_item fired[3];
    size_t n;
};

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

    cfg.text[0] = (struct cfg_stream) {.id = 1, .name = "Command1"};
    cfg.n_text = 1U;
    cfg.midi[0] = (struct cfg_stream) {.id = 1, .name = "MIDI1"};
    cfg.n_midi = 1U;
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
