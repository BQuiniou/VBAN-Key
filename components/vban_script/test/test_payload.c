// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "vban_script/vban_script.h"

#include "unity.h"

#include <string.h>

static void assert_span(char const* expected, char const* span, size_t length)
{
    TEST_ASSERT_EQUAL_UINT(strlen(expected), length);
    TEST_ASSERT_EQUAL_MEMORY(expected, span, length);
}

TEST_CASE("parse GPO and Voicemeeter-style assignments", "[vban_script]")
{
    static char const payload[] = "GPO[1] = 1; GPO [12]=0\nStrip(0).mute=1; Bus(3).EQ.on = 0;"
                                  "Strip(1).Label = \"Mic A\"; Command.Restart = 1;";
    struct vs_assignment out[8];
    size_t count;
    char err[128];

    TEST_ASSERT_EQUAL_INT(VS_OK, vban_script_parse_payload(payload, out, 8U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(6U, count);
    assert_span("GPO", out[0].target, out[0].target_len);
    TEST_ASSERT_EQUAL_INT64(1, out[0].index);
    TEST_ASSERT_EQUAL_UINT(0U, out[0].field_len);
    assert_span("1", out[0].value, out[0].value_len);
    TEST_ASSERT_EQUAL_INT64(12, out[1].index);
    assert_span("0", out[1].value, out[1].value_len);
    assert_span("Strip", out[2].target, out[2].target_len);
    TEST_ASSERT_EQUAL_INT64(0, out[2].index);
    assert_span("mute", out[2].field, out[2].field_len);
    assert_span("EQ.on", out[3].field, out[3].field_len);
    assert_span("Mic A", out[4].value, out[4].value_len);
    assert_span("Command", out[5].target, out[5].target_len);
    TEST_ASSERT_EQUAL_INT64(VS_NO_INDEX, out[5].index);
    assert_span("Restart", out[5].field, out[5].field_len);
}

TEST_CASE("malformed payloads are rejected", "[vban_script]")
{
    struct vs_assignment out[2];
    size_t count;
    char err[128];

    TEST_ASSERT_EQUAL_INT(VS_OK, vban_script_parse_payload("", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(0U, count);
    TEST_ASSERT_EQUAL_INT(VS_ERR_SYNTAX, vban_script_parse_payload("GPO[1 = 1;", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(VS_ERR_SYNTAX, vban_script_parse_payload("GPO[1);", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(VS_ERR_SYNTAX, vban_script_parse_payload("GPO[1] 1;", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(VS_ERR_SYNTAX, vban_script_parse_payload("GPO[1] = ;", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(VS_ERR_SYNTAX, vban_script_parse_payload("[1] = 1;", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(VS_ERR_RANGE, vban_script_parse_payload("GPO[-1] = 1;", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(VS_ERR_TOO_MANY, vban_script_parse_payload("a=1; b=2; c=3;", out, 2U, &count, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "byte"));
}
