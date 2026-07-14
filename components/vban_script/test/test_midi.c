// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

// Focused coverage of MIDI compilation in vban_script: every SendMidi form,
// channel boundaries, a long "data" payload (arena streaming), and the error
// paths (range / bad-midi / unknown-stream / arena-full).

#include "vban_script/vban_script.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

static struct config cfg;

static void init_cfg(void)
{
    memset(&cfg, 0, sizeof cfg);
    cfg.midi[0].id = 1;
    cfg.midi[0].name = "MIDI1";
    cfg.n_midi = 1U;
}

static int compile_script(char const* script, struct vs_script* out)
{
    static uint8_t arena_buf[VS_MIDI_ARENA];
    struct vs_arena arena = {arena_buf, sizeof arena_buf, 0U};
    char err[128];

    return vban_script_compile(script, &cfg, &arena, out, err, sizeof err);
}

TEST_CASE("compile MIDI channel messages", "[vban_script]")
{
    struct vs_script s;

    init_cfg();
    // note-on, channel 1 -> status 0x90
    TEST_ASSERT_EQUAL(VS_OK, compile_script("SendMidi(\"vban1\", \"note-on\", 1, 60, 127);", &s));
    TEST_ASSERT_TRUE(s.n == 1U && s.cmd[0].type == VS_MIDI && s.cmd[0].stream_idx == 0);
    TEST_ASSERT_EQUAL_UINT(3U, s.cmd[0].midi_len);
    TEST_ASSERT_EQUAL_MEMORY(((uint8_t const[]) {0x90U, 60U, 127U}), s.cmd[0].midi, 3U);

    // note-off, channel 16 boundary -> status 0x8F
    TEST_ASSERT_EQUAL(VS_OK, compile_script("SendMidi(\"vban1\", \"note-off\", 16, 60, 0);", &s));
    TEST_ASSERT_EQUAL_UINT(3U, s.cmd[0].midi_len);
    TEST_ASSERT_EQUAL_MEMORY(((uint8_t const[]) {0x8FU, 60U, 0U}), s.cmd[0].midi, 3U);

    // ctrl-change -> status 0xB0
    TEST_ASSERT_EQUAL(VS_OK, compile_script("SendMidi(\"vban1\", \"ctrl-change\", 1, 7, 100);", &s));
    TEST_ASSERT_EQUAL_UINT(3U, s.cmd[0].midi_len);
    TEST_ASSERT_EQUAL_MEMORY(((uint8_t const[]) {0xB0U, 7U, 100U}), s.cmd[0].midi, 3U);

    // prg-change, channel 10 -> status 0xC9, 2 bytes
    TEST_ASSERT_EQUAL(VS_OK, compile_script("SendMidi(\"vban1\", \"prg-change\", 10, 5);", &s));
    TEST_ASSERT_EQUAL_UINT(2U, s.cmd[0].midi_len);
    TEST_ASSERT_EQUAL_MEMORY(((uint8_t const[]) {0xC9U, 5U}), s.cmd[0].midi, 2U);
}

TEST_CASE("compile data and multiple MIDI messages", "[vban_script]")
{
    struct vs_script s;

    init_cfg();
    // short SysEx via "data"
    TEST_ASSERT_EQUAL(VS_OK, compile_script("SendMidi(\"vban1\", \"data\", 240, 67, 247);", &s));
    TEST_ASSERT_EQUAL_UINT(3U, s.cmd[0].midi_len);
    TEST_ASSERT_EQUAL_MEMORY(((uint8_t const[]) {0xF0U, 0x43U, 0xF7U}), s.cmd[0].midi, 3U);

    // several MIDI statements in one script
    TEST_ASSERT_EQUAL(
        VS_OK, compile_script(
                   "SendMidi(\"vban1\", \"note-on\", 1, 60, 127);"
                   "SendMidi(\"vban1\", \"ctrl-change\", 2, 10, 64);",
                   &s
               )
    );
    TEST_ASSERT_EQUAL_UINT(2U, s.n);
    TEST_ASSERT_EQUAL_HEX8(0x90U, s.cmd[0].midi[0]);
    TEST_ASSERT_EQUAL_MEMORY(((uint8_t const[]) {0xB1U, 10U, 64U}), s.cmd[1].midi, 3U);
}

TEST_CASE("compile a long MIDI data payload", "[vban_script]")
{
    struct vs_script s;
    int i;

    init_cfg();
    // long "data" payload -> streamed into the arena (no large stack buffer)
    {
        char big[3000];
        int n = 0;

        n += snprintf(big + n, sizeof big - (size_t) n, "SendMidi(\"vban1\", \"data\"");
        for (i = 0; i < 300; ++i)
        {
            n += snprintf(big + n, sizeof big - (size_t) n, ", %d", i & 0xFF);
        }
        (void) snprintf(big + n, sizeof big - (size_t) n, ");");
        TEST_ASSERT_EQUAL(VS_OK, compile_script(big, &s));
        TEST_ASSERT_TRUE(s.n == 1U && s.cmd[0].midi_len == 300U);
        for (i = 0; i < 300; ++i)
        {
            TEST_ASSERT_EQUAL_UINT8((uint8_t) (i & 0xFF), s.cmd[0].midi[i]);
        }
    }
}

TEST_CASE("reject invalid MIDI", "[vban_script]")
{
    struct vs_script s;

    init_cfg();
    TEST_ASSERT_EQUAL(VS_ERR_RANGE, compile_script("SendMidi(\"vban1\", \"note-on\", 1, 60, 128);", &s));
    TEST_ASSERT_EQUAL(VS_ERR_RANGE, compile_script("SendMidi(\"vban1\", \"note-on\", 0, 60, 127);", &s));
    TEST_ASSERT_EQUAL(VS_ERR_RANGE, compile_script("SendMidi(\"vban1\", \"note-on\", 17, 60, 127);", &s));
    TEST_ASSERT_EQUAL(VS_ERR_RANGE, compile_script("SendMidi(\"vban1\", \"data\", 256);", &s));
    TEST_ASSERT_EQUAL(VS_ERR_BAD_MIDI, compile_script("SendMidi(\"vban1\", \"bogus\", 1, 2, 3);", &s));
    TEST_ASSERT_EQUAL(VS_ERR_BAD_MIDI, compile_script("SendMidi(\"vban1\", \"note-on\", 1, 60, 127, 99);", &s));
    TEST_ASSERT_EQUAL(VS_ERR_UNKNOWN_STREAM, compile_script("SendMidi(\"vban9\", \"note-on\", 1, 60, 127);", &s));

    // arena full: a 2-byte arena cannot hold a 4-byte "data" payload
    {
        static uint8_t tiny[2];
        struct vs_arena arena = {tiny, sizeof tiny, 0U};
        char err[64];

        TEST_ASSERT_EQUAL(
            VS_ERR_ARENA_FULL,
            vban_script_compile("SendMidi(\"vban1\", \"data\", 1, 2, 3, 4);", &cfg, &arena, &s, err, sizeof err)
        );
    }
}
