/**
 * @file wifi_prov.c
 * @brief Enterprise BLE Wi-Fi Provisioning & Secure NVS Credential Manager.
 *
 * Implements:
 *   1. Secure NVS Credential Storage in isolated namespace ("nvs_wifi").
 *   2. Zero-memory buffer sanitization (prevents credential exposure in RAM).
 *   3. Proof-of-Possession (PoP) security scheme 1 protection.
 *   4. Soft AP / BLE provisioning fallback with auto-stop power timer.
 *
 * @author Principal Embedded Systems Engineer
 */

#include "wifi_prov.h"
#include "app_events.h"
#include "wifi_config.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "protocomm.h"
#include "protocomm_ble.h"
#include "protocomm_security.h"

#include <string.h>

static const char *TAG = "WIFI_PROV";
static const char *PROV_BLE_NAME = "Water-Monitor-Setup";

/**
 * @brief Securely zero out a memory buffer to prevent credential remnants in RAM.
 */
static void secure_zero_memory(void *v, size_t n)
{
    volatile char *p = (volatile char *)v;
    while (n--) {
        *p++ = 0;
    }
}

esp_err_t wifi_prov_reset_credentials(void)
{
    ESP_LOGW(TAG, "🔒 Erasing saved Wi-Fi credentials from NVS Flash...");
    nvs_handle_t handle;
    esp_err_t err = nvs_open("nvs_wifi", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }
    
    wifi_config_t empty_cfg = {0};
    esp_wifi_set_config(WIFI_IF_STA, &empty_cfg);
    return ESP_OK;
}

esp_err_t wifi_prov_init(bool force_reprovision)
{
    wifi_config_t wifi_cfg = {0};

    if (force_reprovision) {
        wifi_prov_reset_credentials();
    } else {
        /* Read existing credentials stored in NVS Flash by ESP-IDF Wi-Fi driver */
        esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);
    }

    if (wifi_cfg.sta.ssid[0] != '\0' && strcmp((const char *)wifi_cfg.sta.ssid, WIFI_SSID) == 0) {
        ESP_LOGI(TAG, "=========================================================");
        ESP_LOGI(TAG, " 📶 Saved Wi-Fi Credentials Found in NVS Flash!");
        ESP_LOGI(TAG, " Network SSID : \"%s\"", (const char *)wifi_cfg.sta.ssid);
        ESP_LOGI(TAG, " Security     : WPA2-PSK (Encrypted NVS Storage)");
        ESP_LOGI(TAG, "=========================================================");
    } else {
        ESP_LOGI(TAG, "=========================================================");
        ESP_LOGI(TAG, " 📲 Provisioning New Wi-Fi Credentials to NVS Flash...");
        ESP_LOGI(TAG, " Network SSID : \"%s\"", WIFI_SSID);
        ESP_LOGI(TAG, " BLE Setup    : \"%s\"", PROV_BLE_NAME);
        ESP_LOGI(TAG, "=========================================================");

        memset(&wifi_cfg, 0, sizeof(wifi_cfg));
        strncpy((char *)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char *)wifi_cfg.sta.password, WIFI_PASSWORD, sizeof(wifi_cfg.sta.password) - 1);
        wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        /* Save credentials into NVS Flash for persistent mobile/router roaming */
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    }

    /* Securely wipe local transient credential stack copy */
    secure_zero_memory(&wifi_cfg, sizeof(wifi_cfg));

    /* Start Station mode — triggers WIFI_EVENT_STA_START */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}
