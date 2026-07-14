// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "sim_kbd.h"

#include <ctype.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ESCAPE_CAPACITY      64U
#define DETECTION_TIMEOUT_MS 200

static struct termios saved_termios;
static bool has_saved;
static bool real_events;
static bool registered_cleanup;
static char escape_body[ESCAPE_CAPACITY];
static size_t escape_length;
static enum { INPUT_PLAIN, INPUT_ESCAPE, INPUT_CSI, INPUT_CSI_DROP } input_state;

static bool parse_number(char const* seq, size_t end, size_t* index, int* value)
{
    int parsed = 0;

    if (*index >= end || !isdigit((unsigned char) seq[*index]))
    {
        return false;
    }
    while (*index < end && isdigit((unsigned char) seq[*index]))
    {
        int const digit = seq[*index] - '0';

        if (parsed > (INT_MAX - digit) / 10)
        {
            return false;
        }
        parsed = (parsed * 10) + digit;
        ++*index;
    }
    *value = parsed;
    return true;
}

bool sim_kbd_parse_csi_u(char const* seq, size_t len, int* keycode, int* modifiers, int* event_type)
{
    size_t index = 0U;
    int code = 0;
    int mods = 1;
    int type = 1;

    if (seq == NULL || keycode == NULL || modifiers == NULL || event_type == NULL || len < 2U || seq[len - 1U] != 'u' ||
        !parse_number(seq, len - 1U, &index, &code))
    {
        return false;
    }
    if (index < len - 1U && seq[index] == ';')
    {
        ++index;
        if (!parse_number(seq, len - 1U, &index, &mods))
        {
            return false;
        }
        if (index < len - 1U && seq[index] == ':')
        {
            ++index;
            if (!parse_number(seq, len - 1U, &index, &type))
            {
                return false;
            }
        }
    }
    if (index != len - 1U || type < 1 || type > 3)
    {
        return false;
    }
    *keycode = code;
    *modifiers = mods;
    *event_type = type;
    return true;
}

static long elapsed_ms(const struct timespec* start)
{
    struct timespec now;

    (void) clock_gettime(CLOCK_MONOTONIC, &now);
    return ((now.tv_sec - start->tv_sec) * 1000L) + ((now.tv_nsec - start->tv_nsec) / 1000000L);
}

static bool detect_protocol(void)
{
    static char const query[] = "\x1b[?u\x1b[c";
    struct timespec start;
    char sequence[ESCAPE_CAPACITY];
    size_t length = 0U;
    enum
    {
        DETECT_PLAIN,
        DETECT_ESCAPE,
        DETECT_CSI
    } state = DETECT_PLAIN;
    bool supported = false;

    (void) write(STDOUT_FILENO, query, sizeof(query) - 1U);
    (void) clock_gettime(CLOCK_MONOTONIC, &start);
    while (elapsed_ms(&start) < DETECTION_TIMEOUT_MS)
    {
        struct pollfd input = {STDIN_FILENO, POLLIN, 0};
        long const remaining = DETECTION_TIMEOUT_MS - elapsed_ms(&start);
        int ready = poll(&input, 1U, remaining > 0L ? (int) remaining : 0);
        unsigned char byte;

        if (ready <= 0 || read(STDIN_FILENO, &byte, 1U) != 1)
        {
            continue;
        }
        if (state == DETECT_PLAIN)
        {
            if (byte == 0x1bU)
            {
                state = DETECT_ESCAPE;
            }
            continue;
        }
        if (state == DETECT_ESCAPE)
        {
            state = byte == '[' ? DETECT_CSI : DETECT_PLAIN;
            length = 0U;
            continue;
        }
        if (length < sizeof(sequence))
        {
            sequence[length++] = (char) byte;
        }
        else
        {
            state = DETECT_PLAIN;
            length = 0U;
            continue;
        }
        if (byte >= 0x40U && byte <= 0x7eU)
        {
            size_t index;
            bool digits = length > 2U && sequence[0] == '?';

            for (index = 1U; digits && index + 1U < length; ++index)
            {
                digits = isdigit((unsigned char) sequence[index]) != 0;
            }
            if (byte == 'u' && digits)
            {
                supported = true;
            }
            if (byte == 'c')
            {
                return supported;
            }
            state = DETECT_PLAIN;
            length = 0U;
        }
    }
    return supported;
}

void sim_kbd_shutdown(void)
{
    static char const pop_flags[] = "\x1b[<u";

    if (real_events)
    {
        (void) write(STDOUT_FILENO, pop_flags, sizeof(pop_flags) - 1U);
        real_events = false;
    }
    if (has_saved)
    {
        (void) tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        has_saved = false;
    }
}

void sim_kbd_init(void)
{
    static char const enable_flags[] = "\x1b[>11u";
    struct termios raw;

    input_state = INPUT_PLAIN;
    escape_length = 0U;
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved_termios) != 0)
    {
        return;
    }
    has_saved = true;
    raw = saved_termios;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    (void) tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    if (!registered_cleanup)
    {
        (void) atexit(sim_kbd_shutdown);
        registered_cleanup = true;
    }
    if (detect_protocol())
    {
        (void) write(STDOUT_FILENO, enable_flags, sizeof(enable_flags) - 1U);
        real_events = true;
    }
}

bool sim_kbd_real_events(void)
{
    return real_events;
}

static bool consume_byte(unsigned char byte, struct sim_kbd_event* out)
{
    int keycode;
    int modifiers;
    int event_type;

    if (input_state == INPUT_PLAIN)
    {
        if (byte == 0x1bU)
        {
            input_state = INPUT_ESCAPE;
        }
        else if (!real_events)
        {
            out->action = SIM_KBD_TAP;
            out->key = (int) byte;
            return true;
        }
        return false;
    }
    if (input_state == INPUT_ESCAPE)
    {
        input_state = byte == '[' ? INPUT_CSI : INPUT_PLAIN;
        escape_length = 0U;
        return false;
    }
    if (input_state == INPUT_CSI_DROP)
    {
        if (byte >= 0x40U && byte <= 0x7eU)
        {
            input_state = INPUT_PLAIN;
        }
        return false;
    }
    if (escape_length >= sizeof(escape_body))
    {
        input_state = INPUT_CSI_DROP;
        escape_length = 0U;
        return false;
    }
    escape_body[escape_length++] = (char) byte;
    if (byte < 0x40U || byte > 0x7eU)
    {
        return false;
    }
    input_state = INPUT_PLAIN;
    if (real_events && byte == 'u' && sim_kbd_parse_csi_u(escape_body, escape_length, &keycode, &modifiers, &event_type))
    {
        escape_length = 0U;
        if (event_type == 2)
        {
            return false;
        }
        if (event_type == 1 && (keycode == 3 || (keycode == 'c' && ((modifiers - 1) & 4) != 0)))
        {
            out->action = SIM_KBD_QUIT;
            out->key = keycode;
            return true;
        }
        out->action = event_type == 3 ? SIM_KBD_UP : SIM_KBD_DOWN;
        out->key = keycode;
        return true;
    }
    escape_length = 0U;
    return false;
}

bool sim_kbd_poll(struct sim_kbd_event* out)
{
    unsigned char byte;

    if (out == NULL)
    {
        return false;
    }
    out->action = SIM_KBD_NONE;
    out->key = 0;
    while (read(STDIN_FILENO, &byte, 1U) == 1)
    {
        if (consume_byte(byte, out))
        {
            return true;
        }
    }
    return false;
}
