// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "config/config.h"

#include <stdio.h>
#include <string.h>

static void set_error(char* err, size_t err_len, char const* message)
{
    if (err != NULL && err_len != 0U)
    {
        (void) snprintf(err, err_len, "%s", message);
    }
}

static int fail(struct config* cfg, int result, char* err, size_t err_len, char const* message)
{
    set_error(err, err_len, message);
    toml_free(cfg->toml);
    memset(cfg, 0, sizeof *cfg);
    return result;
}

static int get_optional_string(toml_datum_t table, char const* key, char const** value)
{
    toml_datum_t const datum = toml_get(table, key);

    if (datum.type == TOML_UNKNOWN)
    {
        *value = NULL;
        return CONFIG_OK;
    }
    if (datum.type != TOML_STRING)
    {
        return CONFIG_ERR_SCHEMA;
    }
    *value = datum.u.s;
    return CONFIG_OK;
}

static int get_optional_int(toml_datum_t table, char const* key, int64_t* value)
{
    toml_datum_t const datum = toml_get(table, key);

    if (datum.type == TOML_UNKNOWN)
    {
        *value = 0;
        return CONFIG_OK;
    }
    if (datum.type != TOML_INT64)
    {
        return CONFIG_ERR_SCHEMA;
    }
    *value = datum.u.int64;
    return CONFIG_OK;
}

static int get_optional_bool(toml_datum_t table, char const* key, bool default_value, bool* value)
{
    toml_datum_t const datum = toml_get(table, key);

    if (datum.type == TOML_UNKNOWN)
    {
        *value = default_value;
        return CONFIG_OK;
    }
    if (datum.type != TOML_BOOLEAN)
    {
        return CONFIG_ERR_SCHEMA;
    }
    *value = datum.u.boolean;
    return CONFIG_OK;
}

static int map_wifi(toml_datum_t wifi, struct config* cfg)
{
    toml_datum_t const plaintext_password = toml_get(wifi, "password");

    if (plaintext_password.type != TOML_UNKNOWN ||
        get_optional_string(wifi, "ssid", &cfg->wifi_ssid) != CONFIG_OK ||
        get_optional_bool(wifi, "dhcp", true, &cfg->wifi_dhcp) != CONFIG_OK ||
        get_optional_string(wifi, "ip_address", &cfg->wifi_ip) != CONFIG_OK ||
        get_optional_string(wifi, "netmask", &cfg->wifi_netmask) != CONFIG_OK ||
        get_optional_string(wifi, "gateway", &cfg->wifi_gateway) != CONFIG_OK ||
        get_optional_string(wifi, "dns_server", &cfg->wifi_dns) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (!cfg->wifi_dhcp &&
        (cfg->wifi_ip == NULL || cfg->wifi_netmask == NULL || cfg->wifi_gateway == NULL))
    {
        return CONFIG_ERR_SCHEMA;
    }
    return CONFIG_OK;
}

static int map_log_level(toml_datum_t global, enum log_level* level)
{
    toml_datum_t const datum = toml_get(global, "log_level");

    *level = LOG_INFO;
    if (datum.type == TOML_UNKNOWN)
    {
        return CONFIG_OK;
    }
    if (datum.type != TOML_STRING)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (strcmp(datum.u.s, "fatal") == 0)
    {
        *level = LOG_FATAL;
    }
    else if (strcmp(datum.u.s, "error") == 0)
    {
        *level = LOG_ERROR;
    }
    else if (strcmp(datum.u.s, "warning") == 0)
    {
        *level = LOG_WARNING;
    }
    else if (strcmp(datum.u.s, "info") == 0)
    {
        *level = LOG_INFO;
    }
    else if (strcmp(datum.u.s, "debug") == 0)
    {
        *level = LOG_DEBUG;
    }
    else
    {
        return CONFIG_ERR_SCHEMA;
    }
    return CONFIG_OK;
}

static int map_stream(toml_datum_t table, struct cfg_stream* stream)
{
    int64_t value;

    if (get_optional_int(table, "id", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    stream->id = (int) value;
    if (get_optional_string(table, "ip_address", &stream->ip) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (get_optional_int(table, "port", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    stream->port = (uint16_t) value;
    return get_optional_string(table, "stream_name", &stream->name);
}

static int map_stream_array(toml_datum_t root, char const* key, struct cfg_stream* streams, size_t* count)
{
    toml_datum_t const array = toml_get(root, key);
    int32_t index;

    *count = 0U;
    if (array.type == TOML_UNKNOWN)
    {
        return CONFIG_OK;
    }
    if (array.type != TOML_ARRAY)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (array.u.arr.size > CFG_MAX_STREAMS)
    {
        return CONFIG_ERR_TOO_MANY;
    }
    for (index = 0; index < array.u.arr.size; ++index)
    {
        if (array.u.arr.elem[index].type != TOML_TABLE || map_stream(array.u.arr.elem[index], &streams[index]) != CONFIG_OK)
        {
            return CONFIG_ERR_SCHEMA;
        }
    }
    *count = (size_t) array.u.arr.size;
    return CONFIG_OK;
}

static int map_button(toml_datum_t table, struct cfg_button* button)
{
    toml_datum_t const type = toml_get(table, "type");
    int64_t value;
    int result;

    if (get_optional_int(table, "id", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    button->id = (int) value;
    if (type.type == TOML_UNKNOWN || (type.type == TOML_STRING && strcmp(type.u.s, "latch") == 0))
    {
        button->type = BUTTON_LATCH;
    }
    else if (type.type == TOML_STRING && strcmp(type.u.s, "momentary") == 0)
    {
        button->type = BUTTON_MOMENTARY;
    }
    else
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (get_optional_int(table, "gpi_pin", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    button->gpi_pin = (int) value;

    result = get_optional_string(table, "init", &button->init);
    if (result != CONFIG_OK)
    {
        return result;
    }
    result = get_optional_string(table, "on", &button->on);
    if (result != CONFIG_OK)
    {
        return result;
    }
    return get_optional_string(table, "off", &button->off);
}

static int map_buttons(toml_datum_t root, struct config* cfg)
{
    toml_datum_t const array = toml_get(root, "button");
    int32_t index;
    int result;

    if (array.type == TOML_UNKNOWN)
    {
        return CONFIG_OK;
    }
    if (array.type != TOML_ARRAY)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (array.u.arr.size > CFG_MAX_BUTTONS)
    {
        return CONFIG_ERR_TOO_MANY;
    }
    for (index = 0; index < array.u.arr.size; ++index)
    {
        if (array.u.arr.elem[index].type != TOML_TABLE)
        {
            return CONFIG_ERR_SCHEMA;
        }
        result = map_button(array.u.arr.elem[index], &cfg->buttons[index]);
        if (result != CONFIG_OK)
        {
            return result;
        }
    }
    cfg->n_buttons = (size_t) array.u.arr.size;
    return CONFIG_OK;
}

static int map_config(struct config* cfg)
{
    toml_datum_t const wifi = toml_get(cfg->toml.toptab, "wifi");
    toml_datum_t const global = toml_get(cfg->toml.toptab, "global");
    int64_t value;
    int result;

    cfg->log_level = LOG_INFO;
    cfg->wifi_dhcp = true;
    if (wifi.type != TOML_UNKNOWN)
    {
        if (wifi.type != TOML_TABLE || map_wifi(wifi, cfg) != CONFIG_OK)
        {
            return CONFIG_ERR_SCHEMA;
        }
    }
    if (global.type != TOML_UNKNOWN)
    {
        if (global.type != TOML_TABLE || get_optional_string(global, "default_ip_address", &cfg->default_ip) != CONFIG_OK ||
            get_optional_int(global, "default_port", &value) != CONFIG_OK)
        {
            return CONFIG_ERR_SCHEMA;
        }
        cfg->default_port = (uint16_t) value;
        if (map_log_level(global, &cfg->log_level) != CONFIG_OK)
        {
            return CONFIG_ERR_SCHEMA;
        }
    }

    result = map_stream_array(cfg->toml.toptab, "text", cfg->text, &cfg->n_text);
    if (result != CONFIG_OK)
    {
        return result;
    }
    result = map_stream_array(cfg->toml.toptab, "midi", cfg->midi, &cfg->n_midi);
    if (result != CONFIG_OK)
    {
        return result;
    }
    return map_buttons(cfg->toml.toptab, cfg);
}

int config_load(char const* toml, int len, struct config* out, char* err, size_t err_len)
{
    toml_result_t result;
    int map_result;

    if (out == NULL)
    {
        set_error(err, err_len, "output configuration is NULL");
        return CONFIG_ERR_SCHEMA;
    }
    memset(out, 0, sizeof *out);
    if (toml == NULL || len < 0)
    {
        set_error(err, err_len, "invalid TOML input");
        return CONFIG_ERR_SCHEMA;
    }
    result = toml_parse(toml, len);
    if (!result.ok)
    {
        set_error(err, err_len, result.errmsg);
        toml_free(result);
        return CONFIG_ERR_PARSE;
    }
    out->toml = result;
    map_result = map_config(out);
    if (map_result == CONFIG_ERR_TOO_MANY)
    {
        return fail(out, map_result, err, err_len, "configuration exceeds a fixed-capacity limit");
    }
    if (map_result != CONFIG_OK)
    {
        return fail(out, CONFIG_ERR_SCHEMA, err, err_len, "invalid configuration schema");
    }
    return CONFIG_OK;
}

void config_free(struct config* cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    if (cfg->toml.ok)
    {
        toml_free(cfg->toml);
    }
    memset(cfg, 0, sizeof *cfg);
}
