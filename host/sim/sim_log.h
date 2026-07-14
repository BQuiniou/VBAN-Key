// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_SIM_LOG_H
#define VBANKEY_SIM_LOG_H

#include <stddef.h>
#include <stdint.h>

void sim_log_init(void);
void sim_log_banner(char const* line);
void sim_log_map(int key, int button_id, int gpi, char const* mode);
void sim_log_event(int button_id, char const* phase);
void sim_log_text(int button_id, char const* stream, char const* payload, size_t len);
void sim_log_midi(int button_id, char const* stream, uint8_t const* bytes, size_t len);
void sim_log_wait(int button_id, uint32_t ms);
void sim_log_cancelled(int button_id);
void sim_log_info(char const* line);

#endif // VBANKEY_SIM_LOG_H
