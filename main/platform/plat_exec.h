// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_EXEC_H
#define PLAT_EXEC_H

#include "config/config.h"
#include "runtime/runtime.h"
#include "vban_net/vban_net.h"
#include "vban_script/vban_script.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * Start one FreeRTOS script worker for each configured button.
 *
 * net may be NULL while networking is unavailable; script sends are then
 * logged without transmission. Return false when the configuration exceeds the
 * platform limit or a worker cannot be created.
 */
bool plat_exec_init(const struct runtime* runtime, const struct config* cfg, struct vban_net* net);

/**
 * Queue a script for one button, replacing any older pending script.
 *
 * The runtime_io callback context is unused. A running Wait() is cancelled when
 * the button receives a newer generation.
 */
void plat_exec_fire(void* ctx, size_t button_index, const struct vs_script* script);

#endif // PLAT_EXEC_H
