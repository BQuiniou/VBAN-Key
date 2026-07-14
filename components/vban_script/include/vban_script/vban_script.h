// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_VBAN_SCRIPT_H
#define VBANKEY_VBAN_SCRIPT_H

#include "config/config.h"

#include <stddef.h>
#include <stdint.h>

#define VS_MAX_CMDS   16   /* max statements (SendText/SendMidi/Wait) in one phase script */
#define VS_MIDI_ARENA 4096 /* shared MIDI byte pool, total across all commands */

struct vs_arena
{
    uint8_t* buf;
    size_t cap;
    size_t used;
};

enum vs_cmd_type
{
    VS_TEXT,
    VS_MIDI,
    VS_WAIT
};

enum vs_result
{
    VS_OK = 0,
    VS_ERR_SYNTAX,
    VS_ERR_UNKNOWN_STREAM,
    VS_ERR_BAD_MIDI,
    VS_ERR_RANGE,
    VS_ERR_TOO_MANY,
    VS_ERR_ARENA_FULL,
    VS_ERR_TEXT_TOO_LONG
};

struct vs_command
{
    enum vs_cmd_type type;
    int stream_idx;
    char const* text;
    size_t text_len;
    uint8_t const* midi; /* VS_MIDI: points into the arena */
    uint16_t midi_len;
    uint32_t wait_ms;
};

struct vs_script
{
    struct vs_command cmd[VS_MAX_CMDS];
    size_t n;
};

struct vs_ops
{
    void* ctx;
    void (*send_text)(void* ctx, int stream_idx, char const* payload, size_t len);
    void (*send_midi)(void* ctx, int stream_idx, uint8_t const* bytes, size_t len);
    int (*wait)(void* ctx, uint32_t ms); /* return non-zero to ABORT the running script */
};

/**
 * Compile a phase script into out. script must be a NUL-terminated string (the
 * parser uses the terminating NUL as its bound); it may be NULL or empty. MIDI
 * bytes are written into arena, which is shared across calls; the caller sets
 * used to zero before the first call. TEXT payloads point into script, so script
 * must outlive out. On error, err receives a diagnostic containing a byte offset
 * when space is available.
 */
int vban_script_compile(
    char const* script, const struct config* cfg, struct vs_arena* arena, struct vs_script* out, char* err, size_t err_len
);

/** Execute compiled commands in order through the injected operations. */
void vban_script_run(const struct vs_script* s, const struct vs_ops* ops);

#endif // VBANKEY_VBAN_SCRIPT_H
