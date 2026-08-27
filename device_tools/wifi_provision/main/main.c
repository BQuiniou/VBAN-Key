// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_credentials.h"

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define COMMAND_CAPACITY 200U
#define WIFI_SSID_CAPACITY 33U

static int hex_nibble(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F')
    {
        return value - 'A' + 10;
    }
    return -1;
}

static bool decode_hex(char const* encoded, char* decoded, size_t capacity, size_t* decoded_length)
{
    size_t encoded_length;
    size_t index;

    encoded_length = strlen(encoded);
    if ((encoded_length % 2U) != 0U || encoded_length / 2U >= capacity)
    {
        return false;
    }

    *decoded_length = encoded_length / 2U;
    for (index = 0U; index < *decoded_length; ++index)
    {
        int const high = hex_nibble(encoded[index * 2U]);
        int const low = hex_nibble(encoded[index * 2U + 1U]);

        if (high < 0 || low < 0)
        {
            return false;
        }
        decoded[index] =
            (char) (((unsigned int) high << 4U) | (unsigned int) low);
        if (decoded[index] == '\0')
        {
            return false;
        }
    }
    decoded[*decoded_length] = '\0';
    return true;
}

static void clear_password(char* password, size_t capacity)
{
    volatile unsigned char* byte = (volatile unsigned char*) password;
    size_t index;

    for (index = 0U; index < capacity; ++index)
    {
        byte[index] = 0U;
    }
}

static bool valid_password_length(size_t length)
{
    return length == 0U || (length >= 8U && length <= 63U) || length == 64U;
}

static void discard_line(void)
{
    int value;

    do
    {
        value = getchar();
    } while (value != '\n' && value != EOF);
}

static bool read_command(char* command, size_t capacity)
{
    size_t length;

    if (fgets(command, (int) capacity, stdin) == NULL)
    {
        clearerr(stdin);
        return false;
    }

    length = strlen(command);
    if (length == 0U || command[length - 1U] != '\n')
    {
        discard_line();
        puts("ERROR command too long");
        return false;
    }

    command[--length] = '\0';
    if (length != 0U && command[length - 1U] == '\r')
    {
        command[length - 1U] = '\0';
    }
    return true;
}

static void process_command(char* command)
{
    char password[PLAT_WIFI_PASSWORD_CAPACITY];
    char ssid[WIFI_SSID_CAPACITY];
    char* encoded_password;
    char* separator;
    size_t password_length;
    size_t ssid_length;
    bool valid;

    if (strncmp(command, "SET ", 4U) != 0)
    {
        puts("ERROR expected SET <ssid-hex> <password-hex-or-dash>");
        return;
    }

    separator = strchr(command + 4U, ' ');
    if (separator == NULL)
    {
        puts("ERROR expected SET <ssid-hex> <password-hex-or-dash>");
        return;
    }
    *separator = '\0';
    encoded_password = separator + 1;

    valid = decode_hex(command + 4U, ssid, sizeof ssid, &ssid_length);
    if (strcmp(encoded_password, "-") == 0)
    {
        password[0] = '\0';
        password_length = 0U;
    }
    else
    {
        valid = valid &&
            decode_hex(encoded_password, password, sizeof password, &password_length);
    }

    if (!valid || ssid_length == 0U || !valid_password_length(password_length))
    {
        clear_password(password, sizeof password);
        puts("ERROR invalid SSID or password");
        return;
    }

    if (!plat_credentials_store(ssid, password))
    {
        clear_password(password, sizeof password);
        puts("ERROR credential storage failed");
        return;
    }

    clear_password(password, sizeof password);
    puts("OK");
}

void app_main(void)
{
    char command[COMMAND_CAPACITY];
    usb_serial_jtag_driver_config_t usb_config =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();

    if (usb_serial_jtag_driver_install(&usb_config) != ESP_OK)
    {
        puts("FATAL USB console initialization failed");
        return;
    }
    usb_serial_jtag_vfs_use_driver();

    (void) setvbuf(stdin, NULL, _IONBF, 0);
    (void) setvbuf(stdout, NULL, _IONBF, 0);

    puts("VBANKEY-PROVISION/1");
    if (!plat_credentials_init())
    {
        puts("FATAL encrypted NVS initialization failed");
        return;
    }
    puts("READY");

    for (;;)
    {
        if (read_command(command, sizeof command))
        {
            process_command(command);
        }
    }
}
