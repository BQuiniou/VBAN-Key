// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "sim_exec.h"
#include "sim_kbd.h"
#include "sim_log.h"

#include "button/button.h"
#include "config/config.h"
#include "runtime/runtime.h"
#include "vban_net/vban_net.h"

#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Config files are a few KiB; reject anything absurd. Also bounds load_file's
// malloc so a bogus ftell() cannot overflow the (size + 1) allocation.
#define MAX_CONFIG_BYTES (1024L * 1024L)

struct sim_state
{
    struct runtime* rt;
    const struct config* cfg;
    struct sim_exec* ex;
    bool pressed[CFG_MAX_BUTTONS];
};

static struct runtime runtime_instance;
static struct vban_net net;
static sig_atomic_t volatile g_quit = 0;

static void on_sigint(int sig)
{
    (void) sig;
    g_quit = 1;
}

static bool read_pressed(void* ctx, int gpi_pin)
{
    struct sim_state* state = ctx;
    size_t index;

    for (index = 0U; index < state->rt->n; ++index)
    {
        if (state->rt->btn[index].gpi_pin == gpi_pin)
        {
            return state->pressed[index];
        }
    }
    return false;
}

static uint32_t now_ms(void* ctx)
{
    struct timespec now;

    (void) ctx;
    (void) clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t) (((uint64_t) now.tv_sec * 1000U) + ((uint64_t) now.tv_nsec / 1000000U));
}

static void fire_script(void* ctx, size_t button_index, const struct vs_script* script)
{
    struct sim_state* state = ctx;
    const struct live_button* button = &state->rt->btn[button_index];
    char const* phase = script == &button->init ? "INIT" : script == &button->on ? "ON" : "OFF";

    sim_exec_fire(state->ex, button_index, state->cfg->buttons[button_index].id, phase, script);
}

static char* load_file(char const* path, size_t* length)
{
    FILE* file;
    long size;
    char* buffer;

    file = fopen(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        (void) fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0 || size > MAX_CONFIG_BYTES || fseek(file, 0L, SEEK_SET) != 0)
    {
        (void) fclose(file);
        return NULL;
    }
    buffer = malloc((size_t) size + 1U);
    if (buffer == NULL || fread(buffer, 1U, (size_t) size, file) != (size_t) size)
    {
        free(buffer);
        (void) fclose(file);
        return NULL;
    }
    // NOLINTNEXTLINE(clang-analyzer-security.ArrayBound): size is bounded to [0, MAX_CONFIG_BYTES] above; buffer holds size + 1 bytes
    buffer[size] = '\0';
    *length = (size_t) size;
    (void) fclose(file);
    return buffer;
}

static void demo_tick_after(struct sim_state* state, const struct runtime_io* io, useconds_t delay)
{
    usleep(delay);
    runtime_tick(state->rt, io);
}

static void run_demo(struct sim_state* state, const struct runtime_io* io)
{
    sim_log_info("demo: trigger button 0 (latch)");
    state->pressed[0] = true;
    runtime_tick(state->rt, io);
    demo_tick_after(state, io, (BUTTON_DEBOUNCE_MS + 5U) * 1000U);
    usleep(80000U);

    state->pressed[0] = false;
    runtime_tick(state->rt, io);
    demo_tick_after(state, io, (BUTTON_DEBOUNCE_MS + 5U) * 1000U);
    state->pressed[0] = true;
    runtime_tick(state->rt, io);
    demo_tick_after(state, io, (BUTTON_DEBOUNCE_MS + 5U) * 1000U);

    usleep(40000U);
    state->pressed[0] = false;
    runtime_tick(state->rt, io);
    demo_tick_after(state, io, (BUTTON_DEBOUNCE_MS + 5U) * 1000U);
    state->pressed[0] = true;
    runtime_tick(state->rt, io);
    demo_tick_after(state, io, (BUTTON_DEBOUNCE_MS + 5U) * 1000U);

    usleep(400000U);
    sim_log_info("demo done");
}

int main(int argc, char** argv)
{
    char const* config_path = "config/examples/config.toml";
    bool demo = false;
    bool send = false;
    size_t config_length;
    char* config_buffer;
    struct config cfg;
    char error[256];
    struct sim_exec exec;
    struct sim_state state;
    struct runtime_io io;
    size_t index;
    int arg;

    for (arg = 1; arg < argc; ++arg)
    {
        if (strcmp(argv[arg], "--demo") == 0)
        {
            demo = true;
        }
        else if (strcmp(argv[arg], "--send") == 0)
        {
            send = true;
        }
        else if (argv[arg][0] != '-')
        {
            config_path = argv[arg];
        }
    }
    sim_log_init();
    config_buffer = load_file(config_path, &config_length);
    if (config_buffer == NULL)
    {
        (void) fprintf(stderr, "cannot read config: %s\n", config_path);
        return 1;
    }
    if (config_load(config_buffer, (int) config_length, &cfg, error, sizeof(error)) != CONFIG_OK)
    {
        (void) fprintf(stderr, "config error: %s\n", error);
        free(config_buffer);
        return 1;
    }
    if (runtime_build(&runtime_instance, &cfg, error, sizeof(error)) != RUNTIME_OK)
    {
        (void) fprintf(stderr, "runtime error: %s\n", error);
        config_free(&cfg);
        free(config_buffer);
        return 1;
    }

    if (send && vban_net_open(&net, &cfg) != 0)
    {
        (void) fprintf(stderr, "cannot open VBAN UDP socket\n");
        config_free(&cfg);
        free(config_buffer);
        return 1;
    }
    sim_exec_init(&exec, runtime_instance.n, &cfg, send ? &net : NULL);
    state = (struct sim_state) {&runtime_instance, &cfg, &exec, {false}};
    io = (struct runtime_io) {&state, read_pressed, now_ms, fire_script};

    sim_log_banner("VBAN-Key native simulator");
    if (send)
    {
        printf("[INFO] real sending enabled; default target %s:%u\n", cfg.default_ip, (unsigned int) cfg.default_port);
    }
    else
    {
        printf("[INFO] real sending disabled; default target %s:%u\n", cfg.default_ip, (unsigned int) cfg.default_port);
    }
    for (index = 0U; index < runtime_instance.n; ++index)
    {
        sim_log_map(
            'a' + (int) index, cfg.buttons[index].id, cfg.buttons[index].gpi_pin,
            cfg.buttons[index].type == BUTTON_LATCH ? "latch" : "momentary"
        );
    }
    runtime_start(&runtime_instance, &io);

    if (demo)
    {
        usleep(10000U);
        run_demo(&state, &io);
    }
    else
    {
        (void) signal(SIGINT, on_sigint);
        sim_kbd_init();
        if (sim_kbd_real_events())
        {
            sim_log_info("keyboard: kitty protocol — hold a button's letter (real key up/down); Ctrl-C to quit");
        }
        else
        {
            sim_log_info("keyboard: letter = ON, Shift+letter = OFF (no key-release on this terminal); Ctrl-C to quit");
        }
        while (!g_quit)
        {
            struct sim_kbd_event event;

            while (sim_kbd_poll(&event))
            {
                int base;
                int button;

                if (event.action == SIM_KBD_QUIT)
                {
                    g_quit = 1;
                    break;
                }
                base = tolower((unsigned char) event.key);
                button = base - 'a';
                if (button >= 0 && button < (int) runtime_instance.n)
                {
                    index = (size_t) button;
                    if (event.action == SIM_KBD_DOWN)
                    {
                        state.pressed[index] = true;
                    }
                    else if (event.action == SIM_KBD_UP)
                    {
                        state.pressed[index] = false;
                    }
                    else if (event.action == SIM_KBD_TAP)
                    {
                        // Fallback (no key-release): letter = ON, Shift+letter = OFF; fire the script directly.
                        fire_script(
                            &state, index,
                            isupper((unsigned char) event.key) ? &runtime_instance.btn[index].off
                                                               : &runtime_instance.btn[index].on
                        );
                    }
                }
            }
            runtime_tick(&runtime_instance, &io);
            usleep(3000U);
        }
        sim_kbd_shutdown();
    }

    sim_exec_shutdown(&exec);
    if (send)
    {
        vban_net_close(&net);
    }
    config_free(&cfg);
    free(config_buffer);
    return 0;
}
