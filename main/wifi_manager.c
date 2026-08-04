/**
 * @file wifi_manager.c
 * @brief Wi-Fi Station mode manager — connects ESP32-C6 to home router.
 *
 * Uses the ESP-IDF event-loop WiFi API. On successful IP acquisition,
 * sets EVT_WIFI_CONNECTED in g_system_event_group so dependent tasks
 * (Blynk client) can begin network operations.
 *
 * Reconnection strategy: infinite retry with 3-second back-off.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "wifi_manager.h"
#include "app_events.h"
#include "wifi_config.h"
#include "wifi_prov.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

/* Cached IP string for wifi_manager_get_ip() */
static char s_ip_str[16] = "0.0.0.0";

/* Dual Router Configuration Table */
typedef struct {
    const char *ssid;
    const char *password;
} wifi_net_entry_t;

static const wifi_net_entry_t DUAL_NETWORKS[] = {
    { WIFI_PRIMARY_SSID,   WIFI_PRIMARY_PASSWORD },
    { WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD },
};

static size_t s_net_index = 0;
static int s_disconnect_count = 0;

static void switch_to_next_network(void)
{
    s_net_index = (s_net_index + 1) % 2;
    s_disconnect_count = 0;

    const wifi_net_entry_t *target = &DUAL_NETWORKS[s_net_index];
    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, " 🔄 AUTO-FAILOVER: Switching to Network #%d: \"%s\"", (int)s_net_index + 1, target->ssid);
    ESP_LOGI(TAG, "=========================================================");

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, target->ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, target->password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_connect();
}

/* =========================================================================
 * Internal event handler
 * ========================================================================= */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "Wi-Fi STA started — connecting to Router #%d: \"%s\"...",
               (int)s_net_index + 1, DUAL_NETWORKS[s_net_index].ssid);
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disc =
          (wifi_event_sta_disconnected_t *)event_data;
      s_disconnect_count++;
      ESP_LOGW(TAG, "Disconnected from \"%s\" (reason %d, attempt %d/3)...",
               DUAL_NETWORKS[s_net_index].ssid, disc->reason, s_disconnect_count);
      
      /* Clear connected bit so Blynk/Sinric know link is down */
      xEventGroupClearBits(g_system_event_group, EVT_WIFI_CONNECTED);

      if (s_disconnect_count >= 3) {
          switch_to_next_network();
      } else {
          vTaskDelay(pdMS_TO_TICKS(2000));
          esp_wifi_connect();
      }
      break;
    }

    default:
      break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_str, sizeof(s_ip_str));
    s_disconnect_count = 0;

    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, " 🟢 Wi-Fi Connected to \"%s\"!", DUAL_NETWORKS[s_net_index].ssid);
    ESP_LOGI(TAG, " Assigned IP Address : %s", s_ip_str);
    ESP_LOGI(TAG, "=========================================================");
    
    /* Initialize SNTP for valid timestamps */
    if (!esp_sntp_enabled()) {
      esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
      esp_sntp_setservername(0, "pool.ntp.org");
      esp_sntp_init();
    }
    
    xEventGroupSetBits(g_system_event_group, EVT_WIFI_CONNECTED);
  }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

esp_err_t wifi_manager_init(void) {
  esp_err_t ret;

  /* NVS is required by the Wi-Fi driver to store calibration data */
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS partition truncated — erasing and reinitialising");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  /* TCP/IP adapter and default event loop */
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  /* Wi-Fi driver init with default config */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* Register event handlers */
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  /* Delegate Wi-Fi credentials setup and BLE provisioning to wifi_prov */
  ESP_ERROR_CHECK(wifi_prov_init(false));

  ESP_LOGI(TAG, "Wi-Fi manager initialised — waiting for connection...");
  return ESP_OK;
}

void wifi_manager_get_ip(char *buf, size_t len) {
  strncpy(buf, s_ip_str, len - 1);
  buf[len - 1] = '\0';
}
