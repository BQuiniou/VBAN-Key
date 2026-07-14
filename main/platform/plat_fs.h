// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_FS_H
#define PLAT_FS_H

// ESP-IDF filesystem shim: mount the LittleFS 'storage' partition and read config.
// TODO: read_config(buf, cap) once the config loader exists.
void plat_fs_init(void);

#endif // PLAT_FS_H
