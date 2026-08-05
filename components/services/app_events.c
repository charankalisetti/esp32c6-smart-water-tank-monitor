/**
 * @file app_events.c
 * @brief Allocation and initialization of shared FreeRTOS IPC objects.
 *
 * @author Principal Embedded Systems Engineer
 */

#include "app_events.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "APP_EVENTS";

/* Storage for dedicated subscriber queues */
QueueHandle_t     g_audio_queue         = NULL;
QueueHandle_t     g_blynk_queue         = NULL;
QueueHandle_t     g_sinric_queue        = NULL;
EventGroupHandle_t g_system_event_group = NULL;
SemaphoreHandle_t  g_tls_handshake_mutex = NULL;

volatile water_level_t g_current_level  = WATER_LEVEL_EMPTY;

esp_err_t app_events_init(void)
{
    g_audio_queue = xQueueCreate(APP_LEVEL_QUEUE_DEPTH, sizeof(level_event_t));
    g_blynk_queue = xQueueCreate(APP_LEVEL_QUEUE_DEPTH, sizeof(level_event_t));
    g_sinric_queue = xQueueCreate(APP_LEVEL_QUEUE_DEPTH, sizeof(level_event_t));

    if (!g_audio_queue || !g_blynk_queue || !g_sinric_queue) {
        ESP_LOGE(TAG, "Failed to create subscriber queues — out of heap");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Dedicated queues created: Audio, Blynk, Sinric");

    g_system_event_group = xEventGroupCreate();
    if (g_system_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create system_event_group");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "System event group created");

    g_tls_handshake_mutex = xSemaphoreCreateMutex();
    if (g_tls_handshake_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create g_tls_handshake_mutex");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "TLS handshake serialization mutex created");

    return ESP_OK;
}
