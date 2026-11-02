// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "config/config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "runtime/runtime.h"

#include "plat_exec.h"
#include "plat_fs.h"
#include "plat_gpio.h"
#include "plat_net.h"
#include "plat_time.h"

#include <stddef.h>
#include <stdint.h>

#define CONFIG_BUFFER_CAPACITY   (16U * 1024U)
#define ERROR_BUFFER_CAPACITY    256U
#define RUNTIME_POLL_INTERVAL_MS 10U
#define TEXT_IN_STACK_SIZE       6144U
#define TEXT_IN_PRIORITY         5U
#define TEXT_IN_TIMEOUT_MS       1000U

static char const* TAG = "vban-key";
static char config_buffer[CONFIG_BUFFER_CAPACITY];
static char text_in_buffer[VBAN_MAX_PAYLOAD_SIZE + 1U];
static struct config config_instance;
static struct runtime runtime_instance;
static struct vban_net network_instance;

static bool runtime_read_pressed(void* ctx, int gpi_pin)
{
    (void) ctx;
    return plat_gpio_read_pressed(gpi_pin);
}

static uint32_t runtime_now_ms(void* ctx)
{
    (void) ctx;
    return plat_time_millis();
}

static void runtime_write_gpo(void* ctx, const struct cfg_gpo* gpo, bool high)
{
    (void) ctx;
    ESP_LOGI(TAG, "GPO %d WRITE %s (GPIO%d)", gpo->id, high ? "HIGH" : "LOW", gpo->gpo_pin);
    plat_gpio_write(gpo->gpo_pin, high);
}

static struct runtime_io const runtime_io = {
    .ctx = NULL,
    .read_pressed = runtime_read_pressed,
    .now_ms = runtime_now_ms,
    .fire = plat_exec_fire,
    .write_gpo = runtime_write_gpo,
};

static void text_in_task(void* ctx)
{
    char error[ERROR_BUFFER_CAPACITY];

    (void) ctx;
    for (;;)
    {
        int const stream = vban_net_receive_text(&network_instance, TEXT_IN_TIMEOUT_MS, text_in_buffer, sizeof(text_in_buffer));

        if (stream < 0)
        {
            continue;
        }
        error[0] = '\0';
        if (runtime_receive_text(&runtime_instance, &runtime_io, text_in_buffer, error, sizeof(error)) != RUNTIME_OK ||
            error[0] != '\0')
        {
            ESP_LOGW(TAG, "Text stream %s: %s", config_instance.text_in[stream].name, error);
        }
    }
}

static bool configure_gpos(void)
{
    size_t index;

    for (index = 0U; index < config_instance.n_gpos; ++index)
    {
        struct cfg_gpo const* gpo = &config_instance.gpos[index];

        if (!plat_gpio_configure_output(gpo->gpo_pin, runtime_gpo_level(gpo, false)))
        {
            ESP_LOGE(TAG, "Cannot configure GPO %d on GPIO%d", gpo->id, gpo->gpo_pin);
            return false;
        }
        ESP_LOGI(TAG, "GPO %d: GPIO%d", gpo->id, gpo->gpo_pin);
    }
    return true;
}

void app_main(void)
{
    char error[ERROR_BUFFER_CAPACITY];
    TickType_t last_wake;
    size_t config_length;
    size_t index;

    ESP_LOGI(TAG, "VBAN-Key starting");

    if (!plat_fs_init())
    {
        ESP_LOGE(TAG, "Configuration filesystem initialization failed");
        return;
    }
    if (!plat_fs_read_config(config_buffer, sizeof(config_buffer), &config_length))
    {
        ESP_LOGE(TAG, "Configuration file loading failed");
        return;
    }
    if (config_load(config_buffer, (int) config_length, &config_instance, error, sizeof(error)) != CONFIG_OK)
    {
        ESP_LOGE(TAG, "Configuration parsing failed: %s", error);
        return;
    }
    if (config_instance.n_buttons > PLAT_GPIO_MAX_BUTTONS)
    {
        ESP_LOGE(
            TAG, "Configuration has %u buttons; this platform supports at most %u", (unsigned int) config_instance.n_buttons,
            (unsigned int) PLAT_GPIO_MAX_BUTTONS
        );
        config_free(&config_instance);
        return;
    }
    if (runtime_build(&runtime_instance, &config_instance, error, sizeof(error)) != RUNTIME_OK)
    {
        ESP_LOGE(TAG, "Runtime initialization failed: %s", error);
        config_free(&config_instance);
        return;
    }

    plat_gpio_init();
    for (index = 0U; index < runtime_instance.n; ++index)
    {
        struct live_button const* button = &runtime_instance.btn[index];

        if (!plat_gpio_configure_input(button->gpi_pin))
        {
            ESP_LOGE(TAG, "Cannot configure button %d on GPIO%d", config_instance.buttons[index].id, button->gpi_pin);
            config_free(&config_instance);
            return;
        }
        ESP_LOGI(
            TAG, "Button %d: GPIO%d, %s", config_instance.buttons[index].id, button->gpi_pin,
            config_instance.buttons[index].type == BUTTON_LATCH ? "latch" : "momentary"
        );
    }
    if (!configure_gpos())
    {
        config_free(&config_instance);
        return;
    }

    ESP_LOGI(TAG, "Configuration loaded: %u button(s)", (unsigned int) runtime_instance.n);
    if (!plat_net_init(&config_instance, &network_instance))
    {
        ESP_LOGE(TAG, "Network initialization failed");
        config_free(&config_instance);
        return;
    }

    if (!plat_exec_init(&runtime_instance, &config_instance, &network_instance))
    {
        ESP_LOGE(TAG, "Script executor initialization failed");
        config_free(&config_instance);
        return;
    }

    runtime_start(&runtime_instance, &runtime_io);
    ESP_LOGI(TAG, "Runtime started with a %u ms polling interval", (unsigned int) RUNTIME_POLL_INTERVAL_MS);
    if (config_instance.n_text_in > 0U &&
        xTaskCreate(text_in_task, "text-in", TEXT_IN_STACK_SIZE, NULL, TEXT_IN_PRIORITY, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Cannot create the text receive task");
    }

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        runtime_tick(&runtime_instance, &runtime_io);
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RUNTIME_POLL_INTERVAL_MS));
    }
}
