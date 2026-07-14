// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "sim_log.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int use_color;

static char const* color(char const* code)
{
    return use_color != 0 ? code : "";
}

void sim_log_init(void)
{
    use_color = isatty(STDOUT_FILENO);
}

void sim_log_banner(char const* line)
{
    printf("%s%s%s\n", color("\033[1m"), line, color("\033[0m"));
}

void sim_log_map(int key, int button_id, int gpi, char const* mode)
{
    printf("%s[key '%c']%s button %d  gpi %d  %s\n", color("\033[1;34m"), key, color("\033[0m"), button_id, gpi, mode);
}

void sim_log_event(int button_id, char const* phase)
{
    char const* shade = "\033[1;34m";

    if (strcmp(phase, "ON") == 0)
    {
        shade = "\033[1;32m";
    }
    else if (strcmp(phase, "OFF") == 0)
    {
        shade = "\033[1;33m";
    }
    printf("%s  ● btn %d ▸ %s%s\n", color(shade), button_id, phase, color("\033[0m"));
}

void sim_log_text(int button_id, char const* stream, char const* payload, size_t len)
{
    size_t index;
    int pending_space = 0;

    printf("%s    btn %d SEND TEXT  %s  ", color("\033[36m"), button_id, stream);
    for (index = 0U; index < len; ++index)
    {
        unsigned char const ch = (unsigned char) payload[index];

        if (isspace(ch) != 0)
        {
            pending_space = index != 0U;
        }
        else
        {
            if (pending_space != 0)
            {
                putchar(' ');
            }
            putchar((int) ch);
            pending_space = 0;
        }
    }
    printf("%s\n", color("\033[0m"));
}

void sim_log_midi(int button_id, char const* stream, uint8_t const* bytes, size_t len)
{
    size_t index;

    printf("%s    btn %d SEND MIDI  %s ", color("\033[35m"), button_id, stream);
    for (index = 0U; index < len; ++index)
    {
        printf(" %02X", bytes[index]);
    }
    printf("%s\n", color("\033[0m"));
}

void sim_log_wait(int button_id, uint32_t ms)
{
    printf("%s    btn %d WAIT %ums%s\n", color("\033[2m"), button_id, ms, color("\033[0m"));
}

void sim_log_cancelled(int button_id)
{
    printf("%s    btn %d CANCELLED%s\n", color("\033[1;31m"), button_id, color("\033[0m"));
}

void sim_log_info(char const* line)
{
    printf("%s%s%s\n", color("\033[1;34m"), line, color("\033[0m"));
}
