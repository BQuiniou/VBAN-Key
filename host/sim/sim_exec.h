// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_SIM_EXEC_H
#define VBANKEY_SIM_EXEC_H

#include "common/bitset.h"
#include "config/config.h"
#include "vban_net/vban_net.h"
#include "vban_script/vban_script.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

enum sim_worker_flag
{
    SIM_WORKER_HAS_PENDING,
    SIM_WORKER_QUIT
};

struct sim_worker
{
    pthread_t thread;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    const struct vs_script* pending;
    int pending_id;
    char pending_phase[8];
    uint8_t flags;
    uint64_t gen;
    uint64_t running_gen;
    size_t button_index;
    const struct config* cfg;
    struct vban_net* net;
    int running_id;
};

struct sim_exec
{
    struct sim_worker* workers;
    size_t n;
};

void sim_exec_init(struct sim_exec* ex, size_t n, const struct config* cfg, struct vban_net* net);
void sim_exec_fire(struct sim_exec* ex, size_t button_index, int button_id, char const* phase, const struct vs_script* script);
void sim_exec_shutdown(struct sim_exec* ex);

#endif // VBANKEY_SIM_EXEC_H
