// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "sim_exec.h"

#include "sim_log.h"

#include "vban_net/vban_net.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void send_text(void* ctx, int stream_idx, char const* payload, size_t len)
{
    struct sim_worker* worker = ctx;

    sim_log_text(worker->running_id, worker->cfg->text[stream_idx].name, payload, len);
    if (worker->net != NULL)
    {
        vban_net_send_text(worker->net, stream_idx, payload, len);
    }
}

static void send_midi(void* ctx, int stream_idx, uint8_t const* bytes, size_t len)
{
    struct sim_worker* worker = ctx;

    sim_log_midi(worker->running_id, worker->cfg->midi[stream_idx].name, bytes, len);
    if (worker->net != NULL)
    {
        vban_net_send_midi(worker->net, stream_idx, bytes, len);
    }
}

static int wait_ms(void* ctx, uint32_t ms)
{
    struct sim_worker* worker = ctx;
    struct timespec deadline;
    int result = 0;
    int aborted;

    sim_log_wait(worker->running_id, ms);
    (void) clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += (time_t) (ms / 1000U);
    deadline.tv_nsec += (long) (ms % 1000U) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L)
    {
        ++deadline.tv_sec;
        deadline.tv_nsec -= 1000000000L;
    }

    (void) pthread_mutex_lock(&worker->mtx);
    while (worker->gen == worker->running_gen && result != ETIMEDOUT)
    {
        result = pthread_cond_timedwait(&worker->cv, &worker->mtx, &deadline);
    }
    aborted = worker->gen != worker->running_gen;
    (void) pthread_mutex_unlock(&worker->mtx);
    if (aborted != 0)
    {
        sim_log_cancelled(worker->running_id);
    }
    return aborted != 0 ? 1 : 0;
}

static void* worker_main(void* ctx)
{
    struct sim_worker* worker = ctx;
    const struct vs_ops ops = {worker, send_text, send_midi, wait_ms};

    for (;;)
    {
        const struct vs_script* job;
        int id;
        char phase[8];

        (void) pthread_mutex_lock(&worker->mtx);
        while (!bitset_get(worker->flags, SIM_WORKER_HAS_PENDING) && !bitset_get(worker->flags, SIM_WORKER_QUIT))
        {
            (void) pthread_cond_wait(&worker->cv, &worker->mtx);
        }
        if (bitset_get(worker->flags, SIM_WORKER_QUIT))
        {
            (void) pthread_mutex_unlock(&worker->mtx);
            return NULL;
        }
        job = worker->pending;
        id = worker->pending_id;
        (void) memcpy(phase, worker->pending_phase, sizeof(phase));
        worker->flags = (uint8_t) bitset_put(worker->flags, SIM_WORKER_HAS_PENDING, false);
        worker->running_gen = worker->gen;
        worker->running_id = id;
        (void) pthread_mutex_unlock(&worker->mtx);

        sim_log_event(id, phase);
        vban_script_run(job, &ops);
    }
}

void sim_exec_init(struct sim_exec* ex, size_t n, const struct config* cfg, struct vban_net* net)
{
    size_t index;

    ex->workers = calloc(n, sizeof(*ex->workers));
    ex->n = n;
    if (ex->workers == NULL)
    {
        ex->n = 0U;
        return;
    }
    for (index = 0U; index < n; ++index)
    {
        struct sim_worker* worker = &ex->workers[index];

        worker->button_index = index;
        worker->cfg = cfg;
        worker->net = net;
        (void) pthread_mutex_init(&worker->mtx, NULL);
        (void) pthread_cond_init(&worker->cv, NULL);
        (void) pthread_create(&worker->thread, NULL, worker_main, worker);
    }
}

void sim_exec_fire(struct sim_exec* ex, size_t button_index, int button_id, char const* phase, const struct vs_script* script)
{
    struct sim_worker* worker;

    if (button_index >= ex->n)
    {
        return;
    }
    worker = &ex->workers[button_index];
    (void) pthread_mutex_lock(&worker->mtx);
    worker->pending = script;
    worker->pending_id = button_id;
    (void) strncpy(worker->pending_phase, phase, sizeof(worker->pending_phase) - 1U);
    worker->pending_phase[sizeof(worker->pending_phase) - 1U] = '\0';
    ++worker->gen;
    worker->flags = (uint8_t) bitset_put(worker->flags, SIM_WORKER_HAS_PENDING, true);
    (void) pthread_cond_signal(&worker->cv);
    (void) pthread_mutex_unlock(&worker->mtx);
}

void sim_exec_shutdown(struct sim_exec* ex)
{
    size_t index;

    for (index = 0U; index < ex->n; ++index)
    {
        struct sim_worker* worker = &ex->workers[index];

        (void) pthread_mutex_lock(&worker->mtx);
        worker->flags = (uint8_t) bitset_put(worker->flags, SIM_WORKER_QUIT, true);
        (void) pthread_cond_broadcast(&worker->cv);
        (void) pthread_mutex_unlock(&worker->mtx);
    }
    for (index = 0U; index < ex->n; ++index)
    {
        struct sim_worker* worker = &ex->workers[index];

        (void) pthread_join(worker->thread, NULL);
        (void) pthread_cond_destroy(&worker->cv);
        (void) pthread_mutex_destroy(&worker->mtx);
    }
    free(ex->workers);
    ex->workers = NULL;
    ex->n = 0U;
}
