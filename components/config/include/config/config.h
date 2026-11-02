// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_CONFIG_H
#define VBANKEY_CONFIG_H

#include "common/button_mode.h"
#include "tomlc17.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CFG_MAX_STREAMS       8
#define CFG_MAX_BUTTONS       32
#define CFG_MAX_GPOS          16
#define CFG_VERSION_MIN_MAJOR 1
#define CFG_VERSION_MAJOR     2

enum log_level
{
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
};

enum config_result
{
    CONFIG_OK = 0,
    CONFIG_ERR_PARSE,
    CONFIG_ERR_SCHEMA,
    CONFIG_ERR_TOO_MANY,
};

struct cfg_stream
{
    int id;
    char const* ip;
    uint16_t port;
    char const* name;
};

struct cfg_gpo
{
    int id;
    int gpo_pin;
    bool active_low;
};

struct cfg_button
{
    int id;
    enum button_mode type;
    int gpi_pin;
    int gpo_id;
    char const* init;
    char const* on;
    char const* off;
};

struct config
{
    char const* wifi_ssid;
    bool wifi_dhcp;
    char const* wifi_ip;
    char const* wifi_netmask;
    char const* wifi_gateway;
    char const* wifi_dns;
    char const* default_ip;
    uint16_t default_port;
    enum log_level log_level;
    struct cfg_stream text_out[CFG_MAX_STREAMS];
    size_t n_text_out;
    struct cfg_stream midi_out[CFG_MAX_STREAMS];
    size_t n_midi_out;
    struct cfg_stream text_in[CFG_MAX_STREAMS];
    size_t n_text_in;
    struct cfg_gpo gpos[CFG_MAX_GPOS];
    size_t n_gpos;
    struct cfg_button buttons[CFG_MAX_BUTTONS];
    size_t n_buttons;
    toml_result_t toml; /* Retained backing store; freed by config_free(). */
};

/**
 * Parse and retain a configuration. toml must be NUL-terminated and len must
 * exclude that terminator. On failure, err receives a diagnostic when space is
 * available.
 */
int config_load(char const* toml, int len, struct config* out, char* err, size_t err_len);

/** Free the retained TOML tree and clear the configuration model. */
void config_free(struct config* cfg);

const struct cfg_gpo* config_find_gpo(const struct config* cfg, int64_t id);

#endif // VBANKEY_CONFIG_H
