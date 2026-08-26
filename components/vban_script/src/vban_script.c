// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "vban_script/vban_script.h"

#include "vban/vban.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

struct parser
{
    char const* base;
    char const* pos;
    const struct config* cfg;
    struct vs_arena* arena;
    struct vs_script* out;
    char* err;
    size_t err_len;
};

static uint8_t* arena_alloc(struct vs_arena* arena, size_t size)
{
    uint8_t* allocation;

    if (arena == NULL || arena->buf == NULL || arena->used > arena->cap || size > arena->cap - arena->used)
    {
        return NULL;
    }
    allocation = arena->buf + arena->used;
    arena->used += size;
    return allocation;
}

static int fail_at(struct parser* parser, int result, char const* at, char const* message)
{
    if (parser->err != NULL && parser->err_len != 0U)
    {
        (void) snprintf(parser->err, parser->err_len, "%s at byte %zu", message, (size_t) (at - parser->base));
    }
    return result;
}

static void skip_space(struct parser* parser)
{
    while (*parser->pos != '\0' && isspace((unsigned char) *parser->pos) != 0)
    {
        ++parser->pos;
    }
}

static void skip_between_statements(struct parser* parser)
{
    for (;;)
    {
        skip_space(parser);
        /* Short-circuit keeps this in bounds: pos[1] is read only when pos[0]
         * is '/', so on a NUL-terminated script pos[1] is at worst the NUL. */
        if (parser->pos[0] != '/' || parser->pos[1] != '/')
        {
            return;
        }
        parser->pos += 2;
        while (*parser->pos != '\0' && *parser->pos != '\n')
        {
            ++parser->pos;
        }
    }
}

static int expect_char(struct parser* parser, char expected, char const* message)
{
    skip_space(parser);
    if (*parser->pos != expected)
    {
        return fail_at(parser, VS_ERR_SYNTAX, parser->pos, message);
    }
    ++parser->pos;
    return VS_OK;
}

static int parse_quoted(struct parser* parser, char const** value, size_t* length)
{
    char const* start;

    skip_space(parser);
    if (*parser->pos != '"')
    {
        return fail_at(parser, VS_ERR_SYNTAX, parser->pos, "expected quoted string");
    }
    ++parser->pos;
    start = parser->pos;
    while (*parser->pos != '\0' && *parser->pos != '"')
    {
        ++parser->pos;
    }
    if (*parser->pos != '"')
    {
        return fail_at(parser, VS_ERR_SYNTAX, parser->pos, "unterminated quoted string");
    }
    *value = start;
    *length = (size_t) (parser->pos - start);
    ++parser->pos;
    return VS_OK;
}

static int parse_integer(struct parser* parser, int64_t* value)
{
    char const* start;
    uint64_t magnitude = 0U;
    uint64_t limit;
    int negative = 0;

    skip_space(parser);
    start = parser->pos;
    if (*parser->pos == '+' || *parser->pos == '-')
    {
        negative = *parser->pos == '-';
        ++parser->pos;
    }
    if (isdigit((unsigned char) *parser->pos) == 0)
    {
        return fail_at(parser, VS_ERR_SYNTAX, start, "expected integer");
    }
    limit = negative != 0 ? (uint64_t) INT64_MAX + 1U : (uint64_t) INT64_MAX;
    while (isdigit((unsigned char) *parser->pos) != 0)
    {
        unsigned int const digit = (unsigned int) (*parser->pos - '0');

        if (magnitude > (limit - digit) / 10U)
        {
            return fail_at(parser, VS_ERR_RANGE, start, "integer out of range");
        }
        magnitude = (magnitude * 10U) + digit;
        ++parser->pos;
    }
    if (negative != 0)
    {
        *value = magnitude == (uint64_t) INT64_MAX + 1U ? INT64_MIN : -(int64_t) magnitude;
    }
    else
    {
        *value = (int64_t) magnitude;
    }
    return VS_OK;
}

static int parse_stream_id(struct parser* parser, char const* value, size_t length, int* id)
{
    size_t index;
    unsigned int number = 0U;

    if (length <= 4U || memcmp(value, "vban", 4U) != 0)
    {
        return fail_at(parser, VS_ERR_SYNTAX, value, "invalid stream reference");
    }
    for (index = 4U; index < length; ++index)
    {
        unsigned char const character = (unsigned char) value[index];
        unsigned int digit;

        if (isdigit(character) == 0)
        {
            return fail_at(parser, VS_ERR_SYNTAX, value + index, "invalid stream reference");
        }
        digit = (unsigned int) (character - (unsigned char) '0');
        if (number > ((unsigned int) INT_MAX - digit) / 10U)
        {
            return fail_at(parser, VS_ERR_RANGE, value, "stream id out of range");
        }
        number = (number * 10U) + digit;
    }
    *id = (int) number;
    return VS_OK;
}

static int resolve_stream(struct parser* parser, int text_stream, int id, char const* at, int* stream_idx)
{
    const struct cfg_stream* streams;
    size_t count;
    size_t index;

    streams = text_stream != 0 ? parser->cfg->text : parser->cfg->midi;
    count = text_stream != 0 ? parser->cfg->n_text : parser->cfg->n_midi;
    for (index = 0U; index < count; ++index)
    {
        if (streams[index].id == id)
        {
            *stream_idx = (int) index;
            return VS_OK;
        }
    }
    return fail_at(parser, VS_ERR_UNKNOWN_STREAM, at, "unknown stream");
}

static int parse_stream(struct parser* parser, int text_stream, int* stream_idx)
{
    char const* value = NULL;
    size_t length = 0U;
    int id = 0;
    int result;

    result = parse_quoted(parser, &value, &length);
    if (result != VS_OK)
    {
        return result;
    }
    result = parse_stream_id(parser, value, length, &id);
    if (result != VS_OK)
    {
        return result;
    }
    return resolve_stream(parser, text_stream, id, value, stream_idx);
}

static void parse_optional_semicolon(struct parser* parser)
{
    skip_space(parser);
    if (*parser->pos == ';')
    {
        ++parser->pos;
    }
}

static int parse_send_text(struct parser* parser, struct vs_command* command)
{
    char const* payload_start;
    char const* payload_end;
    char const* scan;
    unsigned int depth = 1U;
    int result;

    command->type = VS_TEXT;
    result = parse_stream(parser, 1, &command->stream_idx);
    if (result != VS_OK)
    {
        return result;
    }
    result = expect_char(parser, ',', "expected comma after stream");
    if (result != VS_OK)
    {
        return result;
    }
    payload_start = parser->pos;
    scan = parser->pos;
    while (*scan != '\0')
    {
        if (*scan == '(')
        {
            ++depth;
        }
        else if (*scan == ')')
        {
            --depth;
            if (depth == 0U)
            {
                break;
            }
        }
        ++scan;
    }
    if (*scan == '\0')
    {
        return fail_at(parser, VS_ERR_SYNTAX, scan, "unterminated SendText");
    }
    payload_end = scan;
    while (payload_start < payload_end && isspace((unsigned char) *payload_start) != 0)
    {
        ++payload_start;
    }
    while (payload_end > payload_start && isspace((unsigned char) payload_end[-1]) != 0)
    {
        --payload_end;
    }
    command->text = payload_start;
    command->text_len = (size_t) (payload_end - payload_start);
    if (command->text_len > VBAN_MAX_PAYLOAD_SIZE)
    {
        return fail_at(
            parser, VS_ERR_TEXT_TOO_LONG, payload_start,
            "TEXT payload exceeds the 1436-byte VBAN packet limit (TEXT has no fragmentation)"
        );
    }
    parser->pos = scan + 1;
    parse_optional_semicolon(parser);
    return VS_OK;
}

static int midi_type_is(char const* type, size_t type_len, char const* expected)
{
    size_t const expected_len = strlen(expected);

    return type_len == expected_len && memcmp(type, expected, type_len) == 0;
}

static int parse_channel_midi(struct parser* parser, struct vs_command* command, uint8_t status, size_t expected_args)
{
    int64_t args[3] = {0};
    uint8_t tmp[3];
    uint8_t* midi;
    size_t index;
    int result;

    if (expected_args > sizeof args / sizeof args[0])
    {
        return fail_at(parser, VS_ERR_BAD_MIDI, parser->pos, "internal: too many MIDI arguments");
    }
    for (index = 0U; index < expected_args; ++index)
    {
        result = expect_char(parser, ',', "expected MIDI argument");
        if (result != VS_OK)
        {
            return result;
        }
        result = parse_integer(parser, &args[index]);
        if (result != VS_OK)
        {
            return result;
        }
    }
    skip_space(parser);
    if (*parser->pos != ')')
    {
        return fail_at(parser, VS_ERR_BAD_MIDI, parser->pos, "wrong MIDI argument count");
    }
    if (args[0] < 1 || args[0] > 16)
    {
        return fail_at(parser, VS_ERR_RANGE, parser->pos, "MIDI channel out of range");
    }
    for (index = 1U; index < expected_args; ++index)
    {
        if (args[index] < 0 || args[index] > 127)
        {
            return fail_at(parser, VS_ERR_RANGE, parser->pos, "MIDI value out of range");
        }
    }
    tmp[0] = (uint8_t) (status | (uint8_t) (args[0] - 1));
    for (index = 1U; index < expected_args; ++index)
    {
        tmp[index] = (uint8_t) args[index];
    }
    midi = arena_alloc(parser->arena, expected_args);
    if (midi == NULL)
    {
        return fail_at(parser, VS_ERR_ARENA_FULL, parser->pos, "MIDI arena full");
    }
    memcpy(midi, tmp, expected_args);
    command->midi = midi;
    command->midi_len = (uint16_t) expected_args;
    return VS_OK;
}

static int parse_data_midi(struct parser* parser, struct vs_command* command)
{
    uint8_t* start = NULL;
    size_t count = 0U;
    int result;

    for (;;)
    {
        int64_t value = 0;
        uint8_t* byte;

        skip_space(parser);
        if (*parser->pos == ')')
        {
            break;
        }
        result = expect_char(parser, ',', "expected MIDI argument or close parenthesis");
        if (result != VS_OK)
        {
            return result;
        }
        result = parse_integer(parser, &value);
        if (result != VS_OK)
        {
            return result;
        }
        if (value < 0 || value > 255)
        {
            return fail_at(parser, VS_ERR_RANGE, parser->pos, "MIDI data byte out of range");
        }
        byte = arena_alloc(parser->arena, 1U);
        if (byte == NULL)
        {
            return fail_at(parser, VS_ERR_ARENA_FULL, parser->pos, "MIDI arena full");
        }
        if (start == NULL)
        {
            start = byte;
        }
        *byte = (uint8_t) value;
        ++count;
    }
    if (count == 0U)
    {
        return fail_at(parser, VS_ERR_BAD_MIDI, parser->pos, "MIDI data is empty");
    }
    command->midi = start;
    command->midi_len = (uint16_t) count;
    return VS_OK;
}

static int parse_send_midi(struct parser* parser, struct vs_command* command)
{
    char const* type;
    size_t type_len;
    int result;

    command->type = VS_MIDI;
    result = parse_stream(parser, 0, &command->stream_idx);
    if (result != VS_OK)
    {
        return result;
    }
    result = expect_char(parser, ',', "expected comma after stream");
    if (result != VS_OK)
    {
        return result;
    }
    result = parse_quoted(parser, &type, &type_len);
    if (result != VS_OK)
    {
        return result;
    }
    if (midi_type_is(type, type_len, "note-on") != 0)
    {
        result = parse_channel_midi(parser, command, 0x90U, 3U);
    }
    else if (midi_type_is(type, type_len, "note-off") != 0)
    {
        result = parse_channel_midi(parser, command, 0x80U, 3U);
    }
    else if (midi_type_is(type, type_len, "ctrl-change") != 0)
    {
        result = parse_channel_midi(parser, command, 0xB0U, 3U);
    }
    else if (midi_type_is(type, type_len, "prg-change") != 0)
    {
        result = parse_channel_midi(parser, command, 0xC0U, 2U);
    }
    else if (midi_type_is(type, type_len, "data") != 0)
    {
        result = parse_data_midi(parser, command);
    }
    else
    {
        return fail_at(parser, VS_ERR_BAD_MIDI, type, "unknown MIDI type");
    }
    if (result != VS_OK)
    {
        return result;
    }
    result = expect_char(parser, ')', "expected close parenthesis after SendMidi");
    if (result != VS_OK)
    {
        return result;
    }
    parse_optional_semicolon(parser);
    return VS_OK;
}

static int parse_wait(struct parser* parser, struct vs_command* command)
{
    char const* argument_at;
    int64_t milliseconds;
    int result;

    command->type = VS_WAIT;
    skip_space(parser);
    argument_at = parser->pos;
    result = parse_integer(parser, &milliseconds);
    if (result != VS_OK)
    {
        return result;
    }
    if (milliseconds < 0 || (uint64_t) milliseconds > UINT32_MAX)
    {
        return fail_at(parser, VS_ERR_RANGE, argument_at, "wait value out of range");
    }
    result = expect_char(parser, ')', "expected close parenthesis after Wait");
    if (result != VS_OK)
    {
        return result;
    }
    command->wait_ms = (uint32_t) milliseconds;
    parse_optional_semicolon(parser);
    return VS_OK;
}

static int parse_statement(struct parser* parser, struct vs_command* command)
{
    char const* name = parser->pos;
    size_t name_len;
    int result;

    while (isalpha((unsigned char) *parser->pos) != 0)
    {
        ++parser->pos;
    }
    name_len = (size_t) (parser->pos - name);
    if (name_len == 0U)
    {
        return fail_at(parser, VS_ERR_SYNTAX, name, "expected function name");
    }
    result = expect_char(parser, '(', "expected open parenthesis");
    if (result != VS_OK)
    {
        return result;
    }
    if (name_len == 8U && memcmp(name, "SendText", 8U) == 0)
    {
        return parse_send_text(parser, command);
    }
    if (name_len == 8U && memcmp(name, "SendMidi", 8U) == 0)
    {
        return parse_send_midi(parser, command);
    }
    if (name_len == 4U && memcmp(name, "Wait", 4U) == 0)
    {
        return parse_wait(parser, command);
    }
    return fail_at(parser, VS_ERR_SYNTAX, name, "unknown function");
}

int vban_script_compile(
    char const* script, const struct config* cfg, struct vs_arena* arena, struct vs_script* out, char* err, size_t err_len
)
{
    static char const empty_script[] = "";
    struct parser parser;
    int result;

    if (err != NULL && err_len != 0U)
    {
        err[0] = '\0';
    }
    if (out == NULL)
    {
        if (err != NULL && err_len != 0U)
        {
            (void) snprintf(err, err_len, "output is NULL at byte 0");
        }
        return VS_ERR_SYNTAX;
    }
    memset(out, 0, sizeof *out);
    parser.base = script != NULL ? script : empty_script;
    parser.pos = parser.base;
    parser.cfg = cfg;
    parser.arena = arena;
    parser.out = out;
    parser.err = err;
    parser.err_len = err_len;
    skip_between_statements(&parser);
    if (*parser.pos == '\0')
    {
        return VS_OK;
    }
    if (cfg == NULL)
    {
        return fail_at(&parser, VS_ERR_SYNTAX, parser.pos, "configuration is NULL");
    }
    while (*parser.pos != '\0')
    {
        if (out->n == VS_MAX_CMDS)
        {
            return fail_at(&parser, VS_ERR_TOO_MANY, parser.pos, "too many commands");
        }
        result = parse_statement(&parser, &out->cmd[out->n]);
        if (result != VS_OK)
        {
            return result;
        }
        ++out->n;
        skip_between_statements(&parser);
    }
    return VS_OK;
}

void vban_script_run(const struct vs_script* s, const struct vs_ops* ops)
{
    size_t index;

    if (s == NULL || ops == NULL)
    {
        return;
    }
    for (index = 0U; index < s->n; ++index)
    {
        const struct vs_command* command = &s->cmd[index];

        switch (command->type)
        {
        case VS_TEXT:
            if (ops->send_text != NULL)
            {
                ops->send_text(ops->ctx, command->stream_idx, command->text, command->text_len);
            }
            break;
        case VS_MIDI:
            if (ops->send_midi != NULL)
            {
                ops->send_midi(ops->ctx, command->stream_idx, command->midi, command->midi_len);
            }
            break;
        case VS_WAIT:
            if (ops->wait != NULL)
            {
                if (ops->wait(ops->ctx, command->wait_ms) != 0)
                {
                    return;
                }
            }
            break;
        }
    }
}
