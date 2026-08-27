// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_CREDENTIALS_H
#define PLAT_CREDENTIALS_H

#include <stdbool.h>
#include <stddef.h>

#define PLAT_WIFI_PASSWORD_CAPACITY 65U

enum plat_credentials_result
{
    PLAT_CREDENTIALS_OK = 0,
    PLAT_CREDENTIALS_NOT_FOUND,
    PLAT_CREDENTIALS_SSID_MISMATCH,
    PLAT_CREDENTIALS_ERROR
};

struct plat_wifi_credentials
{
    char password[PLAT_WIFI_PASSWORD_CAPACITY];
    size_t password_length;
};

/**
 * Initialize the default NVS partition.
 *
 * With the project NVS-encryption settings enabled, the first successful call
 * may generate and burn the configured device-local HMAC eFuse key.
 */
bool plat_credentials_init(void);

/**
 * Load the Wi-Fi credential bound to expected_ssid from the "vbankey" NVS
 * namespace.
 *
 * NVS must already have been initialized. The stored SSID is deliberately
 * checked against the non-secret SSID selected by config.toml.
 */
enum plat_credentials_result plat_credentials_load(
    char const* expected_ssid,
    struct plat_wifi_credentials* credentials
);

/**
 * Atomically store one SSID/password pair in the encrypted NVS namespace.
 *
 * ssid must contain 1..32 bytes. password may contain 0..64 bytes.
 */
bool plat_credentials_store(char const* ssid, char const* password);

/** Clear credential material from RAM after esp_wifi_set_config(). */
void plat_credentials_clear(struct plat_wifi_credentials* credentials);

#endif // PLAT_CREDENTIALS_H
