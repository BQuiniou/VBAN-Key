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

static char const* TAG = "vban-key";
static char config_buffer[CONFIG_BUFFER_CAPACITY];
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

void app_main(void)
{
    static struct runtime_io const runtime_io = {
        .ctx = NULL,
        .read_pressed = runtime_read_pressed,
        .now_ms = runtime_now_ms,
        .fire = plat_exec_fire,
    };

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

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        runtime_tick(&runtime_instance, &runtime_io);
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RUNTIME_POLL_INTERVAL_MS));
    }
}
