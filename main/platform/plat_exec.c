// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_exec.h"

#include "plat_gpio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

#define WORKER_STACK_SIZE 4096U
#define WORKER_PRIORITY   5U

struct plat_exec;

struct plat_worker
{
    struct plat_exec* owner;
    portMUX_TYPE lock;
    TaskHandle_t task;
    const struct vs_script* pending;
    char const* pending_phase;
    uint32_t generation;
    uint32_t running_generation;
    size_t button_index;
    int pending_id;
    int running_id;
    bool has_pending;
};

struct plat_exec
{
    struct plat_worker workers[PLAT_GPIO_MAX_BUTTONS];
    const struct runtime* rt;
    const struct config* cfg;
    struct vban_net* net;
    SemaphoreHandle_t net_lock;
    size_t n;
};

static char const* TAG = "plat-exec";
static struct plat_exec executor;

static char const* script_phase(const struct live_button* button, const struct vs_script* script)
{
    if (script == &button->init)
    {
        return "INIT";
    }
    if (script == &button->on)
    {
        return "ON";
    }
    return "OFF";
}

static bool worker_superseded(struct plat_worker* worker)
{
    bool superseded;

    taskENTER_CRITICAL(&worker->lock);
    superseded = worker->generation != worker->running_generation;
    taskEXIT_CRITICAL(&worker->lock);
    return superseded;
}

static void send_text(void* ctx, int stream_index, char const* payload, size_t length)
{
    struct plat_worker* worker = ctx;
    struct plat_exec* exec = worker->owner;

    ESP_LOGI(TAG, "Button %d SEND TEXT stream %d (%u bytes)", worker->running_id, stream_index, (unsigned int) length);
    if (exec->net == NULL)
    {
        return;
    }
    (void) xSemaphoreTake(exec->net_lock, portMAX_DELAY);
    vban_net_send_text(exec->net, stream_index, payload, length);
    (void) xSemaphoreGive(exec->net_lock);
}

static void send_midi(void* ctx, int stream_index, uint8_t const* bytes, size_t length)
{
    struct plat_worker* worker = ctx;
    struct plat_exec* exec = worker->owner;

    ESP_LOGI(TAG, "Button %d SEND MIDI stream %d (%u bytes)", worker->running_id, stream_index, (unsigned int) length);
    if (exec->net == NULL)
    {
        return;
    }
    (void) xSemaphoreTake(exec->net_lock, portMAX_DELAY);
    vban_net_send_midi(exec->net, stream_index, bytes, length);
    (void) xSemaphoreGive(exec->net_lock);
}

static int wait_ms(void* ctx, uint32_t milliseconds)
{
    struct plat_worker* worker = ctx;
    TickType_t ticks;

    ESP_LOGI(TAG, "Button %d WAIT %ums", worker->running_id, (unsigned int) milliseconds);
    if (worker_superseded(worker))
    {
        ESP_LOGI(TAG, "Button %d CANCELLED", worker->running_id);
        return 1;
    }
    if (milliseconds == 0U)
    {
        return 0;
    }
    ticks = pdMS_TO_TICKS(milliseconds);
    if (ticks == 0U)
    {
        ticks = 1U;
    }
    (void) ulTaskNotifyTake(pdTRUE, ticks);
    if (worker_superseded(worker))
    {
        ESP_LOGI(TAG, "Button %d CANCELLED", worker->running_id);
        return 1;
    }
    return 0;
}

static bool take_pending(struct plat_worker* worker, const struct vs_script** script, char const** phase)
{
    bool available;

    taskENTER_CRITICAL(&worker->lock);
    available = worker->has_pending;
    if (available)
    {
        *script = worker->pending;
        *phase = worker->pending_phase;
        worker->running_generation = worker->generation;
        worker->running_id = worker->pending_id;
        worker->has_pending = false;
    }
    taskEXIT_CRITICAL(&worker->lock);
    return available;
}

static void worker_main(void* ctx)
{
    struct plat_worker* worker = ctx;
    const struct vs_ops operations = {worker, send_text, send_midi, wait_ms};

    for (;;)
    {
        const struct vs_script* script;
        char const* phase;

        if (!take_pending(worker, &script, &phase))
        {
            (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        /*
         * Discard notifications belonging to the accepted generation. A newer
         * generation remains visible through worker->generation even if its
         * notification arrives during this clear.
         */
        (void) ulTaskNotifyTake(pdTRUE, 0U);
        ESP_LOGI(TAG, "Button %d %s", worker->running_id, phase);
        vban_script_run(script, &operations);
    }
}

static void delete_workers(size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index)
    {
        if (executor.workers[index].task != NULL)
        {
            vTaskDelete(executor.workers[index].task);
            executor.workers[index].task = NULL;
        }
    }
}

bool plat_exec_init(const struct runtime* runtime, const struct config* cfg, struct vban_net* net)
{
    size_t index;

    if (runtime == NULL || cfg == NULL || runtime->n > PLAT_GPIO_MAX_BUTTONS || runtime->n > cfg->n_buttons)
    {
        return false;
    }
    (void) memset(&executor, 0, sizeof(executor));
    executor.rt = runtime;
    executor.cfg = cfg;
    executor.net = net;
    executor.n = runtime->n;
    if (net != NULL)
    {
        executor.net_lock = xSemaphoreCreateMutex();
        if (executor.net_lock == NULL)
        {
            return false;
        }
    }

    for (index = 0U; index < executor.n; ++index)
    {
        struct plat_worker* worker = &executor.workers[index];

        worker->owner = &executor;
        worker->button_index = index;
        portMUX_INITIALIZE(&worker->lock);
        if (xTaskCreate(worker_main, "button-worker", WORKER_STACK_SIZE, worker, WORKER_PRIORITY, &worker->task) != pdPASS)
        {
            ESP_LOGE(TAG, "Cannot create worker for button %d", cfg->buttons[index].id);
            delete_workers(index);
            if (executor.net_lock != NULL)
            {
                vSemaphoreDelete(executor.net_lock);
                executor.net_lock = NULL;
            }
            executor.n = 0U;
            return false;
        }
    }
    return true;
}

void plat_exec_fire(void* ctx, size_t button_index, const struct vs_script* script)
{
    struct plat_worker* worker;
    TaskHandle_t task;

    (void) ctx;
    if (button_index >= executor.n || script == NULL)
    {
        return;
    }
    worker = &executor.workers[button_index];
    taskENTER_CRITICAL(&worker->lock);
    worker->pending = script;
    worker->pending_phase = script_phase(&executor.rt->btn[button_index], script);
    worker->pending_id = executor.cfg->buttons[button_index].id;
    ++worker->generation;
    worker->has_pending = true;
    task = worker->task;
    taskEXIT_CRITICAL(&worker->lock);
    xTaskNotifyGive(task);
}
