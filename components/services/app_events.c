/**
 * @file app_events.c
 * @brief Allocation and initialization of shared FreeRTOS IPC objects.
 *
 * Owns the actual storage for g_level_change_queue and g_system_event_group.
 * All other modules reference these via the extern declarations in app_events.h.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "app_events.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "APP_EVENTS";

/* =========================================================================
 * Global handle definitions (storage lives here)
 * ========================================================================= */

QueueHandle_t     g_level_change_queue  = NULL;
EventGroupHandle_t g_system_event_group = NULL;
SemaphoreHandle_t  g_tls_handshake_mutex = NULL;

/* Current water level — written only by water_sensor_task, read by blynk_task */
volatile water_level_t g_current_level  = WATER_LEVEL_EMPTY;

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

esp_err_t app_events_init(void)
{
    /* --- Create the level-change queue ---------------------------------- */
    g_level_change_queue = xQueueCreate(APP_LEVEL_QUEUE_DEPTH, sizeof(level_event_t));
    if (g_level_change_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create level_change_queue — out of heap memory");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Level change queue created (depth=%u, item=%u bytes)",
             APP_LEVEL_QUEUE_DEPTH, (unsigned)sizeof(level_event_t));

    /* --- Create the system event group ---------------------------------- */
    g_system_event_group = xEventGroupCreate();
    if (g_system_event_group == NULL) {
        /* Clean up the already-allocated queue before returning */
        vQueueDelete(g_level_change_queue);
        g_level_change_queue = NULL;
        ESP_LOGE(TAG, "Failed to create system_event_group — out of heap memory");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "System event group created");

    /* --- Create the TLS handshake mutex --------------------------------- */
    g_tls_handshake_mutex = xSemaphoreCreateMutex();
    if (g_tls_handshake_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create g_tls_handshake_mutex");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "TLS handshake serialization mutex created");

    return ESP_OK;
}
