// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_fs.h"

#include "esp_littlefs.h"
#include "esp_log.h"

#include <stdio.h>

#define STORAGE_PATH "/storage"
#define CONFIG_PATH  STORAGE_PATH "/config.toml"

static char const* TAG = "plat-fs";

bool plat_fs_init(void)
{
    esp_vfs_littlefs_conf_t const config = {
        .base_path = STORAGE_PATH,
        .partition_label = "storage",
        .partition = NULL,
        .format_if_mount_failed = false,
        .read_only = true,
        .dont_mount = false,
        .grow_on_mount = false,
    };
    esp_err_t const result = esp_vfs_littlefs_register(&config);

    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot mount LittleFS storage partition: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

bool plat_fs_read_config(char* buffer, size_t capacity, size_t* length)
{
    FILE* file;
    long file_size;

    if (buffer == NULL || capacity == 0U || length == NULL)
    {
        return false;
    }
    file = fopen(CONFIG_PATH, "rb");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Cannot open %s", CONFIG_PATH);
        return false;
    }
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        ESP_LOGE(TAG, "Cannot seek %s", CONFIG_PATH);
        (void) fclose(file);
        return false;
    }
    file_size = ftell(file);
    if (file_size < 0L || (size_t) file_size >= capacity)
    {
        ESP_LOGE(TAG, "%s is too large for the configuration buffer", CONFIG_PATH);
        (void) fclose(file);
        return false;
    }
    if (fseek(file, 0L, SEEK_SET) != 0 || fread(buffer, 1U, (size_t) file_size, file) != (size_t) file_size)
    {
        ESP_LOGE(TAG, "Cannot read %s", CONFIG_PATH);
        (void) fclose(file);
        return false;
    }
    buffer[file_size] = '\0';
    *length = (size_t) file_size;
    (void) fclose(file);
    return true;
}
