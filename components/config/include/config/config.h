// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_CONFIG_H
#define VBANKEY_CONFIG_H

#include "common/button_mode.h"
#include "tomlc17.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CFG_MAX_STREAMS 8
#define CFG_MAX_BUTTONS 32

enum log_level
{
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

enum config_result
{
    CONFIG_OK = 0,
    CONFIG_ERR_PARSE,
    CONFIG_ERR_SCHEMA,
    CONFIG_ERR_TOO_MANY
};

struct cfg_stream
{
    int id;
    char const* ip;
    uint16_t port;
    char const* name;
};

struct cfg_button
{
    int id;
    enum button_mode type;
    int gpi_pin;
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
    struct cfg_stream text[CFG_MAX_STREAMS];
    size_t n_text;
    struct cfg_stream midi[CFG_MAX_STREAMS];
    size_t n_midi;
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

#endif // VBANKEY_CONFIG_H
