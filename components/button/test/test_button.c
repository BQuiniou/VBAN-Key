// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "button/button.h"
#include "unity.h"

TEST_CASE("momentary button transitions", "[button]")
{
    struct button button;

    button_init(&button, BUTTON_MOMENTARY);
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 0U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 5U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 10U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 10U + BUTTON_DEBOUNCE_MS - 1U));
    TEST_ASSERT_EQUAL(BUTTON_ENTER_ON, button_update(&button, true, 10U + BUTTON_DEBOUNCE_MS));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 40U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 50U));
    TEST_ASSERT_EQUAL(BUTTON_ENTER_OFF, button_update(&button, false, 50U + BUTTON_DEBOUNCE_MS));
}

TEST_CASE("latch button transitions", "[button]")
{
    struct button button;

    button_init(&button, BUTTON_LATCH);
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 0U));
    TEST_ASSERT_EQUAL(BUTTON_ENTER_ON, button_update(&button, true, BUTTON_DEBOUNCE_MS));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 30U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 30U + BUTTON_DEBOUNCE_MS));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 60U));
    TEST_ASSERT_EQUAL(BUTTON_ENTER_OFF, button_update(&button, true, 60U + BUTTON_DEBOUNCE_MS));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 90U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 90U + BUTTON_DEBOUNCE_MS));
}

TEST_CASE("transient changes are ignored", "[button]")
{
    struct button button;

    button_init(&button, BUTTON_MOMENTARY);
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 100U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, true, 100U + BUTTON_DEBOUNCE_MS - 1U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 100U + BUTTON_DEBOUNCE_MS - 1U));
    TEST_ASSERT_EQUAL(BUTTON_NONE, button_update(&button, false, 200U));
}
