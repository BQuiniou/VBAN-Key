// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "config/config.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_error(char* err, size_t err_len, char const* format, ...)
{
    va_list arguments;

    if (err != NULL && err_len != 0U)
    {
        va_start(arguments, format);
        (void) vsnprintf(err, err_len, format, arguments);
        va_end(arguments);
    }
}

static int fail(struct config* cfg, int result, char* err, size_t err_len, char const* message)
{
    if (err != NULL && err_len != 0U && err[0] == '\0')
    {
        set_error(err, err_len, "%s", message);
    }
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

    if (plaintext_password.type != TOML_UNKNOWN || get_optional_string(wifi, "ssid", &cfg->wifi_ssid) != CONFIG_OK ||
        get_optional_bool(wifi, "dhcp", true, &cfg->wifi_dhcp) != CONFIG_OK ||
        get_optional_string(wifi, "ip_address", &cfg->wifi_ip) != CONFIG_OK ||
        get_optional_string(wifi, "netmask", &cfg->wifi_netmask) != CONFIG_OK ||
        get_optional_string(wifi, "gateway", &cfg->wifi_gateway) != CONFIG_OK ||
        get_optional_string(wifi, "dns_server", &cfg->wifi_dns) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (!cfg->wifi_dhcp && (cfg->wifi_ip == NULL || cfg->wifi_netmask == NULL || cfg->wifi_gateway == NULL))
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

static int map_table_array(
    toml_datum_t root, char const* key, size_t capacity, int (*map_item)(toml_datum_t table, void* items, size_t index),
    void* items, size_t* count
)
{
    toml_datum_t const array = toml_get(root, key);
    int32_t index;
    int result;

    *count = 0U;
    if (array.type == TOML_UNKNOWN)
    {
        return CONFIG_OK;
    }
    if (array.type != TOML_ARRAY)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if ((size_t) array.u.arr.size > capacity)
    {
        return CONFIG_ERR_TOO_MANY;
    }
    for (index = 0; index < array.u.arr.size; ++index)
    {
        if (array.u.arr.elem[index].type != TOML_TABLE)
        {
            return CONFIG_ERR_SCHEMA;
        }
        result = map_item(array.u.arr.elem[index], items, (size_t) index);
        if (result != CONFIG_OK)
        {
            return result;
        }
    }
    *count = (size_t) array.u.arr.size;
    return CONFIG_OK;
}

static int map_stream_item(toml_datum_t table, void* items, size_t index)
{
    return map_stream(table, &((struct cfg_stream*) items)[index]);
}

static int map_stream_array(toml_datum_t root, char const* key, struct cfg_stream* streams, size_t* count)
{
    return map_table_array(root, key, CFG_MAX_STREAMS, map_stream_item, streams, count);
}

static int map_gpo_item(toml_datum_t table, void* items, size_t index)
{
    struct cfg_gpo* gpo = &((struct cfg_gpo*) items)[index];
    int64_t value;

    if (get_optional_int(table, "id", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    gpo->id = (int) value;
    if (get_optional_int(table, "gpo_pin", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    gpo->gpo_pin = (int) value;
    return get_optional_bool(table, "active_low", false, &gpo->active_low);
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
    if (get_optional_int(table, "gpo", &value) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    button->gpo_id = (int) value;

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

static int map_button_item(toml_datum_t table, void* items, size_t index)
{
    return map_button(table, &((struct cfg_button*) items)[index]);
}

static bool parse_version_major(char const* text, int* major)
{
    int part;

    for (part = 0; part < 3; ++part)
    {
        int value = 0;

        if (isdigit((unsigned char) *text) == 0)
        {
            return false;
        }
        while (isdigit((unsigned char) *text) != 0)
        {
            int const digit = *text - '0';

            if (value > (INT_MAX - digit) / 10)
            {
                return false;
            }
            value = (value * 10) + digit;
            ++text;
        }
        if (part == 0)
        {
            *major = value;
        }
        if (part < 2)
        {
            if (*text != '.')
            {
                return false;
            }
            ++text;
        }
    }
    return *text == '\0';
}

static int map_version(toml_datum_t global, int* major, char* err, size_t err_len)
{
    toml_datum_t const datum = toml_get(global, "version");

    *major = 1;
    if (datum.type != TOML_UNKNOWN && (datum.type != TOML_STRING || !parse_version_major(datum.u.s, major)))
    {
        set_error(err, err_len, "[global] version must be \"MAJOR.MINOR.PATCH\"");
        return CONFIG_ERR_SCHEMA;
    }
    if (*major < CFG_VERSION_MIN_MAJOR || *major > CFG_VERSION_MAJOR)
    {
        set_error(
            err, err_len, "config version major %d is not supported; expected %d to %d", *major, CFG_VERSION_MIN_MAJOR,
            CFG_VERSION_MAJOR
        );
        return CONFIG_ERR_SCHEMA;
    }
    return CONFIG_OK;
}

struct version_rule
{
    char const* table;
    char const* key;
    int since;
    int until;
    char const* renamed_to;
};

static const struct version_rule version_rules[] = {
    {NULL, "text", 1, 1, "text_out"},     {NULL, "midi", 1, 1, "midi_out"},    {NULL, "text_out", 2, INT_MAX, NULL},
    {NULL, "midi_out", 2, INT_MAX, NULL}, {NULL, "text_in", 2, INT_MAX, NULL}, {NULL, "gpo", 2, INT_MAX, NULL},
    {"button", "gpo", 2, INT_MAX, NULL},
};

static void format_rule_name(const struct version_rule* rule, char const* key, char* name, size_t name_len)
{
    if (rule->table == NULL)
    {
        (void) snprintf(name, name_len, "[[%s]]", key);
    }
    else
    {
        (void) snprintf(name, name_len, "[[%s]] %s", rule->table, key);
    }
}

static int check_rule(const struct version_rule* rule, toml_datum_t table, int major, char* err, size_t err_len)
{
    char name[48];
    char renamed[48];

    if (toml_get(table, rule->key).type == TOML_UNKNOWN || (major >= rule->since && major <= rule->until))
    {
        return CONFIG_OK;
    }
    format_rule_name(rule, rule->key, name, sizeof name);
    if (major < rule->since)
    {
        set_error(err, err_len, "%s requires config version %d", name, rule->since);
    }
    else if (rule->renamed_to != NULL)
    {
        format_rule_name(rule, rule->renamed_to, renamed, sizeof renamed);
        set_error(err, err_len, "%s was renamed to %s in config version %d", name, renamed, rule->until + 1);
    }
    else
    {
        set_error(err, err_len, "%s was removed in config version %d", name, rule->until + 1);
    }
    return CONFIG_ERR_SCHEMA;
}

static int check_version_rules(toml_datum_t root, int major, char* err, size_t err_len)
{
    size_t index;
    int32_t element;

    for (index = 0U; index < sizeof version_rules / sizeof version_rules[0]; ++index)
    {
        const struct version_rule* rule = &version_rules[index];
        toml_datum_t const array = rule->table != NULL ? toml_get(root, rule->table) : root;

        if (rule->table == NULL)
        {
            if (check_rule(rule, root, major, err, err_len) != CONFIG_OK)
            {
                return CONFIG_ERR_SCHEMA;
            }
            continue;
        }
        for (element = 0; array.type == TOML_ARRAY && element < array.u.arr.size; ++element)
        {
            if (check_rule(rule, array.u.arr.elem[element], major, err, err_len) != CONFIG_OK)
            {
                return CONFIG_ERR_SCHEMA;
            }
        }
    }
    return CONFIG_OK;
}

static char const* section_key(int major, char const* key)
{
    bool renamed = true;
    size_t index;

    while (renamed)
    {
        renamed = false;
        for (index = 0U; index < sizeof version_rules / sizeof version_rules[0] && !renamed; ++index)
        {
            const struct version_rule* rule = &version_rules[index];

            if (rule->table == NULL && rule->renamed_to != NULL && strcmp(rule->renamed_to, key) == 0 && major <= rule->until)
            {
                key = rule->key;
                renamed = true;
            }
        }
    }
    return key;
}

static const struct cfg_gpo* find_gpo(const struct config* cfg, int64_t id, size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        if (cfg->gpos[index].id == id)
        {
            return &cfg->gpos[index];
        }
    }
    return NULL;
}

const struct cfg_gpo* config_find_gpo(const struct config* cfg, int64_t id)
{
    return find_gpo(cfg, id, cfg->n_gpos);
}

static int validate_references(const struct config* cfg, char* err, size_t err_len)
{
    size_t index;

    for (index = 0U; index < cfg->n_text_in; ++index)
    {
        if (cfg->text_in[index].name == NULL || cfg->text_in[index].name[0] == '\0')
        {
            set_error(err, err_len, "[[text_in]] requires a stream_name");
            return CONFIG_ERR_SCHEMA;
        }
    }
    for (index = 0U; index < cfg->n_gpos; ++index)
    {
        if (cfg->gpos[index].id <= 0 || find_gpo(cfg, cfg->gpos[index].id, index) != NULL)
        {
            set_error(err, err_len, "[[gpo]] id %d must be positive and unique", cfg->gpos[index].id);
            return CONFIG_ERR_SCHEMA;
        }
    }
    for (index = 0U; index < cfg->n_buttons; ++index)
    {
        int const gpo_id = cfg->buttons[index].gpo_id;

        if (gpo_id != 0 && find_gpo(cfg, gpo_id, cfg->n_gpos) == NULL)
        {
            set_error(err, err_len, "button %d drives unknown gpo %d", cfg->buttons[index].id, gpo_id);
            return CONFIG_ERR_SCHEMA;
        }
    }
    return CONFIG_OK;
}

static int validate_pins(const struct config* cfg, char* err, size_t err_len)
{
    int pins[CFG_MAX_BUTTONS + CFG_MAX_GPOS];
    size_t n_pins = 0U;
    size_t index;
    size_t other;

    for (index = 0U; index < cfg->n_buttons; ++index)
    {
        pins[n_pins++] = cfg->buttons[index].gpi_pin;
    }
    for (index = 0U; index < cfg->n_gpos; ++index)
    {
        pins[n_pins++] = cfg->gpos[index].gpo_pin;
    }
    for (index = 0U; index < n_pins; ++index)
    {
        for (other = 0U; other < index; ++other)
        {
            if (pins[other] == pins[index])
            {
                set_error(err, err_len, "GPIO%d is assigned more than once across gpi_pin/gpo_pin", pins[index]);
                return CONFIG_ERR_SCHEMA;
            }
        }
    }
    return CONFIG_OK;
}

static int map_config(struct config* cfg, char* err, size_t err_len)
{
    toml_datum_t const root = cfg->toml.toptab;
    toml_datum_t const wifi = toml_get(root, "wifi");
    toml_datum_t const global = toml_get(root, "global");
    int major;
    int64_t value;
    int result;

    cfg->log_level = LOG_INFO;
    cfg->wifi_dhcp = true;
    if (global.type != TOML_UNKNOWN && global.type != TOML_TABLE)
    {
        return CONFIG_ERR_SCHEMA;
    }
    result = map_version(global, &major, err, err_len);
    if (result != CONFIG_OK)
    {
        return result;
    }
    if (check_version_rules(root, major, err, err_len) != CONFIG_OK)
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (wifi.type != TOML_UNKNOWN && (wifi.type != TOML_TABLE || map_wifi(wifi, cfg) != CONFIG_OK))
    {
        return CONFIG_ERR_SCHEMA;
    }
    if (global.type != TOML_UNKNOWN)
    {
        if (get_optional_string(global, "default_ip_address", &cfg->default_ip) != CONFIG_OK ||
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

    result = map_stream_array(root, section_key(major, "text_out"), cfg->text_out, &cfg->n_text_out);
    if (result == CONFIG_OK)
    {
        result = map_stream_array(root, section_key(major, "midi_out"), cfg->midi_out, &cfg->n_midi_out);
    }
    if (result == CONFIG_OK)
    {
        result = map_stream_array(root, section_key(major, "text_in"), cfg->text_in, &cfg->n_text_in);
    }
    if (result == CONFIG_OK)
    {
        result = map_table_array(root, section_key(major, "gpo"), CFG_MAX_GPOS, map_gpo_item, cfg->gpos, &cfg->n_gpos);
    }
    if (result == CONFIG_OK)
    {
        result = map_table_array(
            root, section_key(major, "button"), CFG_MAX_BUTTONS, map_button_item, cfg->buttons, &cfg->n_buttons
        );
    }
    if (result == CONFIG_OK)
    {
        result = validate_references(cfg, err, err_len);
    }
    if (result == CONFIG_OK)
    {
        result = validate_pins(cfg, err, err_len);
    }
    return result;
}

int config_load(char const* toml, int len, struct config* out, char* err, size_t err_len)
{
    toml_result_t result;
    int map_result;

    if (out == NULL)
    {
        set_error(err, err_len, "%s", "output configuration is NULL");
        return CONFIG_ERR_SCHEMA;
    }
    memset(out, 0, sizeof *out);
    set_error(err, err_len, "%s", "");
    if (toml == NULL || len < 0)
    {
        set_error(err, err_len, "%s", "invalid TOML input");
        return CONFIG_ERR_SCHEMA;
    }
    result = toml_parse(toml, len);
    if (!result.ok)
    {
        set_error(err, err_len, "%s", result.errmsg);
        toml_free(result);
        return CONFIG_ERR_PARSE;
    }
    out->toml = result;
    map_result = map_config(out, err, err_len);
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
