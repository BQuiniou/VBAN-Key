// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "plat_net.h"

#include "plat_credentials.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <stddef.h>
#include <string.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT    BIT1
#define WIFI_MAX_RETRIES   5

struct plat_net_state
{
    EventGroupHandle_t events;
    esp_netif_t* station;
    const struct config* cfg;
    int retries;
};

static char const* TAG = "plat-net";
static struct plat_net_state state;

static bool parse_static_address(const struct config* cfg, esp_netif_ip_info_t* address)
{
    return esp_netif_str_to_ip4(cfg->wifi_ip, &address->ip) == ESP_OK &&
        esp_netif_str_to_ip4(cfg->wifi_netmask, &address->netmask) == ESP_OK &&
        esp_netif_str_to_ip4(cfg->wifi_gateway, &address->gw) == ESP_OK;
}

static bool configure_static_address(void)
{
    esp_netif_dns_info_t dns = {0};
    esp_netif_ip_info_t address = {0};
    esp_err_t result;

    if (!parse_static_address(state.cfg, &address))
    {
        ESP_LOGE(TAG, "Invalid static IPv4 configuration");
        return false;
    }

    result = esp_netif_dhcpc_stop(state.station);
    if (result != ESP_OK && result != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
    {
        ESP_LOGE(TAG, "Cannot stop DHCP client: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_netif_set_ip_info(state.station, &address);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot set static IPv4 configuration: %s", esp_err_to_name(result));
        return false;
    }

    if (state.cfg->wifi_dns != NULL)
    {
        dns.ip.type = IPADDR_TYPE_V4;
        if (esp_netif_str_to_ip4(state.cfg->wifi_dns, &dns.ip.u_addr.ip4) != ESP_OK)
        {
            ESP_LOGE(TAG, "Invalid DNS server address");
            return false;
        }
        result = esp_netif_set_dns_info(state.station, ESP_NETIF_DNS_MAIN, &dns);
        if (result != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot set DNS server: %s", esp_err_to_name(result));
            return false;
        }
    }
    return true;
}

static void event_handler(
    void* ctx,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data
)
{
    (void) ctx;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        (void) esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        if (!state.cfg->wifi_dhcp && !configure_static_address())
        {
            (void) xEventGroupSetBits(state.events, WIFI_FAILED_BIT);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (state.retries < WIFI_MAX_RETRIES)
        {
            ++state.retries;
            ESP_LOGW(TAG, "Wi-Fi disconnected; retry %d/%d", state.retries, WIFI_MAX_RETRIES);
            (void) esp_wifi_connect();
        }
        else
        {
            ESP_LOGE(TAG, "Wi-Fi connection retries exhausted");
            (void) xEventGroupSetBits(state.events, WIFI_FAILED_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t const* event = event_data;

        state.retries = 0;
        ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&event->ip_info.ip));
        (void) xEventGroupSetBits(state.events, WIFI_CONNECTED_BIT);
    }
}

static bool load_wifi_config(const struct config* cfg, wifi_config_t* wifi)
{
    struct plat_wifi_credentials credentials;
    enum plat_credentials_result result;
    size_t ssid_length;

    if (cfg->wifi_ssid == NULL || cfg->wifi_ssid[0] == '\0')
    {
        ESP_LOGE(TAG, "No Wi-Fi SSID configured");
        return false;
    }

    result = plat_credentials_load(cfg->wifi_ssid, &credentials);
    if (result == PLAT_CREDENTIALS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "No Wi-Fi credential has been provisioned");
        return false;
    }
    if (result == PLAT_CREDENTIALS_SSID_MISMATCH)
    {
        ESP_LOGE(TAG, "Provisioned credential does not match configured SSID");
        return false;
    }
    if (result != PLAT_CREDENTIALS_OK)
    {
        ESP_LOGE(TAG, "Cannot read Wi-Fi credential from encrypted NVS");
        return false;
    }

    ssid_length = strlen(cfg->wifi_ssid);
    if (ssid_length == 0U || ssid_length > sizeof wifi->sta.ssid ||
        credentials.password_length > sizeof wifi->sta.password)
    {
        plat_credentials_clear(&credentials);
        ESP_LOGE(TAG, "Wi-Fi credential exceeds ESP32-C3 limits");
        return false;
    }

    (void) memset(wifi, 0, sizeof *wifi);
    (void) memcpy(wifi->sta.ssid, cfg->wifi_ssid, ssid_length);
    (void) memcpy(
        wifi->sta.password,
        credentials.password,
        credentials.password_length
    );
    wifi->sta.threshold.authmode =
        credentials.password_length == 0U ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    plat_credentials_clear(&credentials);
    return true;
}

bool plat_net_init(const struct config* cfg, struct vban_net* net)
{
    esp_event_handler_instance_t ip_handler;
    esp_event_handler_instance_t wifi_handler;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi;
    EventBits_t bits;
    esp_err_t result;

    if (cfg == NULL || net == NULL)
    {
        return false;
    }
    (void) memset(&state, 0, sizeof state);
    state.cfg = cfg;

    if (!plat_credentials_init())
    {
        ESP_LOGE(TAG, "Encrypted NVS initialization failed");
        return false;
    }
    if (!load_wifi_config(cfg, &wifi))
    {
        return false;
    }

    state.events = xEventGroupCreate();
    if (state.events == NULL)
    {
        ESP_LOGE(TAG, "Cannot create Wi-Fi event group");
        return false;
    }
    result = esp_netif_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Network stack initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Event loop initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    state.station = esp_netif_create_default_wifi_sta();
    if (state.station == NULL)
    {
        ESP_LOGE(TAG, "Cannot create Wi-Fi station interface");
        return false;
    }
    result = esp_wifi_init(&init);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot select RAM-only Wi-Fi storage: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        event_handler,
        NULL,
        &wifi_handler
    );
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register Wi-Fi event handler: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        event_handler,
        NULL,
        &ip_handler
    );
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot register IP event handler: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result == ESP_OK)
    {
        result = esp_wifi_set_config(WIFI_IF_STA, &wifi);
    }
    (void) memset(&wifi, 0, sizeof wifi);
    if (result == ESP_OK)
    {
        result = esp_wifi_start();
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot start Wi-Fi station: %s", esp_err_to_name(result));
        return false;
    }

    ESP_LOGI(TAG, "Connecting to SSID \"%s\"", cfg->wifi_ssid);
    bits = xEventGroupWaitBits(
        state.events,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );
    if ((bits & WIFI_CONNECTED_BIT) == 0U)
    {
        ESP_LOGE(TAG, "Cannot connect to configured Wi-Fi network");
        return false;
    }
    if (vban_net_open(net, cfg) != 0)
    {
        ESP_LOGE(TAG, "Cannot open VBAN UDP socket");
        return false;
    }

    ESP_LOGI(TAG, "VBAN UDP socket ready");
    return true;
}
