// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "sim_kbd.h"
#include "unity.h"

#include <string.h>

static void expect_event(char const* sequence, int expected_keycode, int expected_modifiers, int expected_event_type)
{
    int keycode;
    int modifiers;
    int event_type;

    TEST_ASSERT_TRUE(sim_kbd_parse_csi_u(sequence, strlen(sequence), &keycode, &modifiers, &event_type));
    TEST_ASSERT_EQUAL_INT(expected_keycode, keycode);
    TEST_ASSERT_EQUAL_INT(expected_modifiers, modifiers);
    TEST_ASSERT_EQUAL_INT(expected_event_type, event_type);
}

TEST_CASE("valid CSI u events are parsed", "[kbd]")
{
    expect_event("49u", 49, 1, 1);
    expect_event("49;1u", 49, 1, 1);
    expect_event("49;1:3u", 49, 1, 3);
    expect_event("113;2:2u", 113, 2, 2);
    expect_event("99;5:1u", 99, 5, 1);
    expect_event("99;5u", 99, 5, 1);
}

TEST_CASE("invalid CSI u events are rejected", "[kbd]")
{
    int keycode;
    int modifiers;
    int event_type;

    TEST_ASSERT_FALSE(sim_kbd_parse_csi_u("49", 2U, &keycode, &modifiers, &event_type));
    TEST_ASSERT_FALSE(sim_kbd_parse_csi_u("", 0U, &keycode, &modifiers, &event_type));
    TEST_ASSERT_FALSE(sim_kbd_parse_csi_u("abc", 3U, &keycode, &modifiers, &event_type));
}
