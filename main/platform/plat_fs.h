// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_FS_H
#define PLAT_FS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Mount the LittleFS storage partition read-only.
 *
 * Return true on success.
 */
bool plat_fs_init(void);

/**
 * Read /config.toml from the mounted storage partition.
 *
 * The file is NUL-terminated in buffer. length receives the number of file
 * bytes, excluding that terminator. Return false on an invalid argument, an I/O
 * error, or when the file does not fit in the supplied buffer.
 */
bool plat_fs_read_config(char* buffer, size_t capacity, size_t* length);

#endif // PLAT_FS_H
