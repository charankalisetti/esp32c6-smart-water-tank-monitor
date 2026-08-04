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

/* =========================================================================
 * Internal event handler
 * ========================================================================= */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "Wi-Fi STA started — connecting to \"%s\"...", WIFI_SSID);
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disc =
          (wifi_event_sta_disconnected_t *)event_data;
      ESP_LOGW(TAG, "Disconnected (reason %d) — retrying in 3 s...",
               disc->reason);
      /* Clear connected bit so Blynk task knows link is down */
      xEventGroupClearBits(g_system_event_group, EVT_WIFI_CONNECTED);
      vTaskDelay(pdMS_TO_TICKS(3000));
      esp_wifi_connect();
      break;
    }

    default:
      break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_str, sizeof(s_ip_str));
    ESP_LOGI(TAG, "Got IP: %s", s_ip_str);
    
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

  /* Configure Station credentials */
  wifi_config_t wifi_cfg = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASSWORD,
              /* Use WPA2/WPA3 automatically */
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  /* Copy credentials safely */
  strncpy((char *)wifi_cfg.sta.ssid, WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
  strncpy((char *)wifi_cfg.sta.password, WIFI_PASSWORD,
          sizeof(wifi_cfg.sta.password) - 1);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_start()); /* Triggers WIFI_EVENT_STA_START */

  ESP_LOGI(TAG, "Wi-Fi manager initialised — waiting for connection...");
  return ESP_OK;
}

void wifi_manager_get_ip(char *buf, size_t len) {
  strncpy(buf, s_ip_str, len - 1);
  buf[len - 1] = '\0';
}
