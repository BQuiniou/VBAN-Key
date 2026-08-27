// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_credentials.h"

#include "nvs.h"
#include "nvs_flash.h"

#include <stdint.h>
#include <string.h>

#define WIFI_SSID_CAPACITY 33U

static char const* NVS_NAMESPACE = "vbankey";
static char const* NVS_WIFI_SSID_KEY = "wifi_ssid";
static char const* NVS_WIFI_PASSWORD_KEY = "wifi_password";

bool plat_credentials_init(void)
{
    /*
     * Do not use the common erase-and-retry fallback here: automatically
     * erasing NVS would silently destroy the provisioned credential.
     */
    return nvs_flash_init() == ESP_OK;
}

enum plat_credentials_result plat_credentials_load(
    char const* expected_ssid,
    struct plat_wifi_credentials* credentials
)
{
    char stored_ssid[WIFI_SSID_CAPACITY];
    nvs_handle_t handle;
    size_t password_capacity;
    size_t ssid_capacity;
    esp_err_t result;

    if (expected_ssid == NULL || expected_ssid[0] == '\0' || credentials == NULL)
    {
        return PLAT_CREDENTIALS_ERROR;
    }
    (void) memset(credentials, 0, sizeof *credentials);

    result = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        return PLAT_CREDENTIALS_NOT_FOUND;
    }
    if (result != ESP_OK)
    {
        return PLAT_CREDENTIALS_ERROR;
    }

    ssid_capacity = sizeof stored_ssid;
    result = nvs_get_str(handle, NVS_WIFI_SSID_KEY, stored_ssid, &ssid_capacity);
    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(handle);
        return PLAT_CREDENTIALS_NOT_FOUND;
    }
    if (result != ESP_OK)
    {
        nvs_close(handle);
        return PLAT_CREDENTIALS_ERROR;
    }
    if (strcmp(expected_ssid, stored_ssid) != 0)
    {
        nvs_close(handle);
        return PLAT_CREDENTIALS_SSID_MISMATCH;
    }

    password_capacity = sizeof credentials->password;
    result = nvs_get_str(
        handle,
        NVS_WIFI_PASSWORD_KEY,
        credentials->password,
        &password_capacity
    );
    nvs_close(handle);

    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        return PLAT_CREDENTIALS_NOT_FOUND;
    }
    if (result != ESP_OK || password_capacity == 0U)
    {
        plat_credentials_clear(credentials);
        return PLAT_CREDENTIALS_ERROR;
    }

    credentials->password_length = password_capacity - 1U;
    return PLAT_CREDENTIALS_OK;
}

bool plat_credentials_store(char const* ssid, char const* password)
{
    nvs_handle_t handle;
    size_t password_length;
    size_t ssid_length;
    esp_err_t result;

    if (ssid == NULL || password == NULL)
    {
        return false;
    }
    ssid_length = strlen(ssid);
    password_length = strlen(password);
    if (ssid_length == 0U || ssid_length >= WIFI_SSID_CAPACITY ||
        password_length >= PLAT_WIFI_PASSWORD_CAPACITY)
    {
        return false;
    }

    result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK)
    {
        return false;
    }
    result = nvs_set_str(handle, NVS_WIFI_SSID_KEY, ssid);
    if (result == ESP_OK)
    {
        result = nvs_set_str(handle, NVS_WIFI_PASSWORD_KEY, password);
    }
    if (result == ESP_OK)
    {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result == ESP_OK;
}

void plat_credentials_clear(struct plat_wifi_credentials* credentials)
{
    volatile uint8_t* byte;
    size_t index;

    if (credentials == NULL)
    {
        return;
    }
    byte = (volatile uint8_t*) credentials;
    for (index = 0U; index < sizeof *credentials; ++index)
    {
        byte[index] = 0U;
    }
}
