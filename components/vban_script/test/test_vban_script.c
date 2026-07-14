// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "vban_script/vban_script.h"

#include "unity.h"

#include <stdio.h>
#include <string.h>

enum log_tag
{
    LOG_TEXT,
    LOG_WAIT,
    LOG_MIDI
};

struct mock_log
{
    enum log_tag tags[3];
    size_t n;
    uint32_t wait_ms;
    int abort_wait;
};

static int payload_contains(char const* payload, size_t payload_len, char const* needle)
{
    size_t const needle_len = strlen(needle);
    size_t index;

    if (needle_len > payload_len)
    {
        return 0;
    }
    for (index = 0U; index <= payload_len - needle_len; ++index)
    {
        if (memcmp(payload + index, needle, needle_len) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void mock_send_text(void* ctx, int stream_idx, char const* payload, size_t len)
{
    struct mock_log* log = ctx;

    TEST_ASSERT_TRUE(stream_idx == 0);
    TEST_ASSERT_TRUE(payload != NULL);
    TEST_ASSERT_TRUE(len != 0U);
    log->tags[log->n++] = LOG_TEXT;
}

static int mock_wait(void* ctx, uint32_t ms)
{
    struct mock_log* log = ctx;

    log->tags[log->n++] = LOG_WAIT;
    log->wait_ms = ms;
    return log->abort_wait;
}

static void mock_send_midi(void* ctx, int stream_idx, uint8_t const* bytes, size_t len)
{
    struct mock_log* log = ctx;

    TEST_ASSERT_TRUE(stream_idx == 0);
    TEST_ASSERT_TRUE(len == 3U);
    TEST_ASSERT_TRUE(bytes[0] == 0x90U);
    TEST_ASSERT_TRUE(bytes[1] == 60U);
    TEST_ASSERT_TRUE(bytes[2] == 127U);
    log->tags[log->n++] = LOG_MIDI;
}

static void init_cfg(struct config* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->text[0].id = 1;
    cfg->text[0].name = "Command1";
    cfg->n_text = 1U;
    cfg->midi[0].id = 1;
    cfg->midi[0].name = "MIDI1";
    cfg->n_midi = 1U;
}

TEST_CASE("over-length SendText is rejected", "[vban_script]")
{
    struct config cfg;
    uint8_t abuf[VS_MIDI_ARENA];
    struct vs_arena arena = {abuf, sizeof abuf, 0U};
    struct vs_script script;
    char big[1600];
    char err[128];
    int n;
    size_t k;

    init_cfg(&cfg);
    n = snprintf(big, sizeof big, "SendText(\"vban1\", ");
    TEST_ASSERT_TRUE(n > 0 && (size_t) n < sizeof big);
    for (k = 0U; k < 1500U; ++k)
    {
        big[n++] = 'x';
    }
    n += snprintf(big + n, sizeof big - (size_t) n, ");");
    TEST_ASSERT_TRUE(n > 0 && (size_t) n < sizeof big);
    TEST_ASSERT_TRUE(vban_script_compile(big, &cfg, &arena, &script, err, sizeof err) == VS_ERR_TEXT_TOO_LONG);
}

TEST_CASE("compile and run a text/wait/midi script in order", "[vban_script]")
{
    static char const source[] = "SendText(\"vban1\",\n"
                                 "  Strip(0).mute = 1;\n"
                                 "  Strip(1).mute = 1;\n"
                                 ");\n"
                                 "Wait(200);\n"
                                 "SendMidi(\"vban1\", \"note-on\", 1, 60, 127);\n";
    struct config cfg;
    uint8_t abuf[VS_MIDI_ARENA];
    struct vs_arena arena = {abuf, sizeof abuf, 0U};
    struct vs_script script;
    struct mock_log log = {0};
    const struct vs_ops ops = {
        .ctx = &log,
        .send_text = mock_send_text,
        .send_midi = mock_send_midi,
        .wait = mock_wait,
    };
    char err[128];

    init_cfg(&cfg);
    TEST_ASSERT_TRUE(vban_script_compile(source, &cfg, &arena, &script, err, sizeof err) == VS_OK);
    TEST_ASSERT_TRUE(script.n == 3U);
    TEST_ASSERT_TRUE(script.cmd[0].type == VS_TEXT);
    TEST_ASSERT_TRUE(script.cmd[0].stream_idx == 0);
    TEST_ASSERT_TRUE(script.cmd[0].text_len > 0U);
    TEST_ASSERT_TRUE(payload_contains(script.cmd[0].text, script.cmd[0].text_len, "Strip(0).mute = 1;") != 0);
    TEST_ASSERT_TRUE(script.cmd[1].type == VS_WAIT);
    TEST_ASSERT_TRUE(script.cmd[1].wait_ms == 200U);
    TEST_ASSERT_TRUE(script.cmd[2].type == VS_MIDI);
    TEST_ASSERT_TRUE(script.cmd[2].stream_idx == 0);
    TEST_ASSERT_TRUE(script.cmd[2].midi_len == 3U);
    TEST_ASSERT_TRUE(script.cmd[2].midi[0] == 0x90U);
    TEST_ASSERT_TRUE(script.cmd[2].midi[1] == 60U);
    TEST_ASSERT_TRUE(script.cmd[2].midi[2] == 127U);

    vban_script_run(&script, &ops);
    TEST_ASSERT_TRUE(log.n == 3U);
    TEST_ASSERT_TRUE(log.tags[0] == LOG_TEXT);
    TEST_ASSERT_TRUE(log.tags[1] == LOG_WAIT);
    TEST_ASSERT_TRUE(log.wait_ms == 200U);
    TEST_ASSERT_TRUE(log.tags[2] == LOG_MIDI);
}

TEST_CASE("SendMidi data payload compiles; a tiny arena overflows", "[vban_script]")
{
    static char const data_source[] = "SendMidi(\"vban1\", \"data\", 240, 1, 2, 247);";
    struct config cfg;
    uint8_t abuf[VS_MIDI_ARENA];
    uint8_t buf2[2];
    struct vs_arena arena = {abuf, sizeof abuf, 0U};
    struct vs_arena small = {buf2, sizeof buf2, 0U};
    struct vs_script script;
    char err[128];

    init_cfg(&cfg);
    TEST_ASSERT_TRUE(vban_script_compile(data_source, &cfg, &arena, &script, err, sizeof err) == VS_OK);
    TEST_ASSERT_TRUE(script.cmd[0].midi_len == 4U);
    TEST_ASSERT_TRUE(memcmp(script.cmd[0].midi, (uint8_t const[]) {240U, 1U, 2U, 247U}, 4U) == 0);

    TEST_ASSERT_TRUE(vban_script_compile(data_source, &cfg, &small, &script, err, sizeof err) == VS_ERR_ARENA_FULL);
}

TEST_CASE("Wait abort cancels the run; a re-run completes", "[vban_script]")
{
    static char const cancellable[] =
        "SendText(\"vban1\", x); Wait(50); SendMidi(\"vban1\", \"note-on\", 1, 60, 127);";
    struct config cfg;
    uint8_t abuf[VS_MIDI_ARENA];
    struct vs_arena arena = {abuf, sizeof abuf, 0U};
    struct vs_script script;
    struct mock_log log = {0};
    const struct vs_ops ops = {
        .ctx = &log,
        .send_text = mock_send_text,
        .send_midi = mock_send_midi,
        .wait = mock_wait,
    };
    char err[128];

    init_cfg(&cfg);
    TEST_ASSERT_TRUE(vban_script_compile(cancellable, &cfg, &arena, &script, err, sizeof err) == VS_OK);

    log.abort_wait = 1;
    vban_script_run(&script, &ops);
    TEST_ASSERT_TRUE(log.n == 2U);
    TEST_ASSERT_TRUE(log.tags[0] == LOG_TEXT);
    TEST_ASSERT_TRUE(log.tags[1] == LOG_WAIT);

    memset(&log, 0, sizeof log);
    vban_script_run(&script, &ops);
    TEST_ASSERT_TRUE(log.n == 3U);
    TEST_ASSERT_TRUE(log.tags[0] == LOG_TEXT);
    TEST_ASSERT_TRUE(log.tags[1] == LOG_WAIT);
    TEST_ASSERT_TRUE(log.tags[2] == LOG_MIDI);
}

TEST_CASE("unknown stream is rejected", "[vban_script]")
{
    static char const unknown_stream[] = "SendText(\"vban9\", x);";
    struct config cfg;
    uint8_t abuf[VS_MIDI_ARENA];
    struct vs_arena arena = {abuf, sizeof abuf, 0U};
    struct vs_script script;
    char err[128];

    init_cfg(&cfg);
    TEST_ASSERT_TRUE(vban_script_compile(unknown_stream, &cfg, &arena, &script, err, sizeof err) == VS_ERR_UNKNOWN_STREAM);
    TEST_ASSERT_TRUE(strstr(err, "byte") != NULL);
}

TEST_CASE("truncated and malformed inputs fail gracefully", "[vban_script]")
{
    struct config cfg;
    uint8_t abuf[VS_MIDI_ARENA];
    struct vs_arena arena = {abuf, sizeof abuf, 0U};
    struct vs_script script;
    char err[128];

    init_cfg(&cfg);
    // Truncated / malformed inputs must fail gracefully, never reading past the NUL.
    TEST_ASSERT_TRUE(vban_script_compile("SendText(", &cfg, &arena, &script, err, sizeof err) != VS_OK);
    TEST_ASSERT_TRUE(vban_script_compile("SendText(\"vban1\"", &cfg, &arena, &script, err, sizeof err) != VS_OK);
    TEST_ASSERT_TRUE(vban_script_compile("SendText(\"vban1\", (", &cfg, &arena, &script, err, sizeof err) != VS_OK);
    TEST_ASSERT_TRUE(
        vban_script_compile("SendMidi(\"vban1\", \"note-on\", 1, 60", &cfg, &arena, &script, err, sizeof err) != VS_OK
    );
    TEST_ASSERT_TRUE(vban_script_compile("Wait(", &cfg, &arena, &script, err, sizeof err) != VS_OK);
    TEST_ASSERT_TRUE(vban_script_compile("/", &cfg, &arena, &script, err, sizeof err) != VS_OK);
    TEST_ASSERT_TRUE(vban_script_compile("//", &cfg, &arena, &script, err, sizeof err) == VS_OK && script.n == 0U);
}
