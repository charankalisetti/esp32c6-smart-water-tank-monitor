/**
 * @file blynk_client.c
 * @brief Blynk IoT HTTPS client — pushes water level data to Blynk cloud.
 *
 * Uses the ESP-IDF esp_http_client component to make HTTPS GET requests
 * to the Blynk HTTP API:
 *
 *   https://blynk.cloud/external/api/update?token=TOKEN&V0=value
 *
 * Push notifications (logEvent) are sent ONLY when the water level genuinely
 * transitions to a new state. Periodic 15-second heartbeats update virtual pins
 * V0-V4 without sending phone push notifications.
 *
 * @author Principal Embedded Systems Engineer
 */

#include "blynk_client.h"
#include "wifi_config.h"
#include "app_events.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"

static const char *TAG = "BLYNK";

typedef struct {
    const char *label;     /* Human-readable status string for V0    */
    int         percent;   /* Water percentage for V1                */
    int         gpio10;    /* Low probe state  for V2 (1=wet, 0=dry) */
    int         gpio11;    /* Med probe state  for V3 (1=wet, 0=dry) */
    int         gpio23;    /* Full probe state for V4 (1=wet, 0=dry) */
} blynk_level_info_t;

static const blynk_level_info_t k_level_info[] = {
    /* WATER_LEVEL_EMPTY   */ { "Tank Empty",        0,   0, 0, 0 },
    /* WATER_LEVEL_LOW     */ { "Water Level Low",  22,   1, 0, 0 },
    /* WATER_LEVEL_MEDIUM  */ { "Water Level 61%",  61,   1, 1, 0 },
    /* WATER_LEVEL_FULL    */ { "Tank Full",        100,  1, 1, 1 },
    /* WATER_LEVEL_INVALID */ { "Sensor Fault",      0,   0, 0, 0 },
};
#define LEVEL_INFO_COUNT  (sizeof(k_level_info) / sizeof(k_level_info[0]))

static void url_encode(char *dst, size_t dst_len, const char *src)
{
    static const char hex[] = "0123456789ABCDEF";
    char *end = dst + dst_len - 1;
    while (*src && dst < end) {
        char c = *src++;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            *dst++ = c;
        } else if (dst + 2 < end) {
            *dst++ = '%';
            *dst++ = hex[(unsigned char)c >> 4];
            *dst++ = hex[(unsigned char)c & 0x0F];
        } else {
            break;
        }
    }
    *dst = '\0';
}

static esp_err_t blynk_https_get(const char *url)
{
    esp_http_client_config_t cfg = {
        .url            = url,
        .method         = HTTP_METHOD_GET,
        .timeout_ms     = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    if (g_tls_handshake_mutex != NULL) {
        xSemaphoreTake(g_tls_handshake_mutex, portMAX_DELAY);
    }

    esp_err_t ret = esp_http_client_perform(client);

    if (g_tls_handshake_mutex != NULL) {
        xSemaphoreGive(g_tls_handshake_mutex);
    }

    if (ret == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            ESP_LOGW(TAG, "Blynk HTTP %d for %s", status, url);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTPS GET failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    return ret;
}

static void blynk_update_pin(const char *pin, const char *value)
{
    char encoded_value[128];
    url_encode(encoded_value, sizeof(encoded_value), value);

    char url[320];
    snprintf(url, sizeof(url),
             "https://" BLYNK_SERVER "/external/api/update"
             "?token=" BLYNK_AUTH_TOKEN "&%s=%s",
             pin, encoded_value);

    esp_err_t ret = blynk_https_get(url);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Updated %s = %s", pin, value);
    }
}

static void blynk_log_event(const char *event_name, const char *description)
{
    char encoded_desc[128];
    url_encode(encoded_desc, sizeof(encoded_desc), description);

    char url[384];
    snprintf(url, sizeof(url),
             "https://" BLYNK_SERVER "/external/api/logEvent"
             "?token=" BLYNK_AUTH_TOKEN "&code=%s&description=%s",
             event_name, encoded_desc);
    esp_err_t ret = blynk_https_get(url);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Notification sent: %s", description);
    }
}

/**
 * @brief Push all 5 virtual pins + optional phone push notification.
 */
static void blynk_push_level(water_level_t level, bool send_notification)
{
    if ((size_t)level >= LEVEL_INFO_COUNT) {
        level = WATER_LEVEL_INVALID;
    }
    const blynk_level_info_t *info = &k_level_info[(int)level];

    char int_str[8];

    /* V0 — Status label */
    blynk_update_pin(BLYNK_PIN_STATUS, info->label);

    /* V1 — Percentage */
    snprintf(int_str, sizeof(int_str), "%d", info->percent);
    blynk_update_pin(BLYNK_PIN_PERCENT, int_str);

    /* V2 — GPIO10 (Low probe) */
    snprintf(int_str, sizeof(int_str), "%d", info->gpio10);
    blynk_update_pin(BLYNK_PIN_GPIO10, int_str);

    /* V3 — GPIO11 (Medium probe) */
    snprintf(int_str, sizeof(int_str), "%d", info->gpio11);
    blynk_update_pin(BLYNK_PIN_GPIO11, int_str);

    /* V4 — GPIO22 (Full probe) */
    snprintf(int_str, sizeof(int_str), "%d", info->gpio23);
    blynk_update_pin(BLYNK_PIN_GPIO22, int_str);

    /* Send phone push notification ONLY on genuine level change events */
    if (send_notification) {
        char desc[64];
        snprintf(desc, sizeof(desc), "%s (%d%%)", info->label, info->percent);
        blynk_log_event(BLYNK_EVENT_LEVEL, desc);
    }

    ESP_LOGI(TAG, "Blynk updated: %s (%d%%) (notify=%s)",
             info->label, info->percent, send_notification ? "YES" : "NO");
}

static void blynk_task(void *arg)
{
    ESP_LOGI(TAG, "Blynk task started — waiting for Wi-Fi...");

    /* Gate on Wi-Fi connected */
    xEventGroupWaitBits(g_system_event_group,
                        EVT_WIFI_CONNECTED,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    ESP_LOGI(TAG, "Wi-Fi ready — pushing initial state to Blynk (no push notification)...");

    /* Push the current level pins immediately on connect (without phone push notification) */
    blynk_push_level(g_current_level, false);

    /* Main loop: block on queue, push on every genuine level change */
    level_event_t evt;
    for (;;) {
        if (xQueueReceive(g_blynk_queue,
                          &evt,
                          pdMS_TO_TICKS(15000)) == pdTRUE) {

            EventBits_t bits = xEventGroupGetBits(g_system_event_group);
            if (!(bits & EVT_WIFI_CONNECTED)) {
                ESP_LOGW(TAG, "Wi-Fi down — skipping Blynk update");
                continue;
            }

            ESP_LOGI(TAG, "Level change event received: %d — pushing to Blynk + notification",
                     (int)evt.level);
            /* Genuine level change -> update 5 pins + send phone push notification */
            blynk_push_level(evt.level, true);
        } else {
            /* Timeout (15 s): Periodic heartbeat — update virtual pins silently (NO push notification) */
            EventBits_t bits = xEventGroupGetBits(g_system_event_group);
            if (bits & EVT_WIFI_CONNECTED) {
                blynk_push_level(g_current_level, false);
                ESP_LOGD(TAG, "Heartbeat sync sent to Blynk (level=%d, notify=NO)", (int)g_current_level);
            }
        }
    }
}

esp_err_t blynk_task_start(void)
{
    BaseType_t ret = xTaskCreate(
        blynk_task,
        "blynk_task",
        8192,
        NULL,
        2,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create blynk_task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "blynk_task created");
    return ESP_OK;
}
