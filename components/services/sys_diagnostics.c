/**
 * @file sys_diagnostics.c
 * @brief Real-time system health diagnostics & telemetry implementation.
 *
 * Runs a background FreeRTOS task every 60 seconds to log heap statistics,
 * Wi-Fi signal RSSI (dBm), and system uptime.
 *
 * @author Principal Embedded Systems Engineer
 */

#include "sys_diagnostics.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DIAGNOSTICS";
static sys_health_metrics_t s_metrics = {0};

static void sys_diagnostics_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System health diagnostics task started (interval=60s)");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));

        s_metrics.free_heap = esp_get_free_heap_size();
        s_metrics.min_free_heap = esp_get_minimum_free_heap_size();
        s_metrics.uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_metrics.wifi_rssi = ap_info.rssi;
        } else {
            s_metrics.wifi_rssi = 0;
        }

        ESP_LOGI(TAG, "=========================================================");
        ESP_LOGI(TAG, " 📊 SYSTEM HEALTH & TELEMETRY REPORT");
        ESP_LOGI(TAG, " Uptime          : %lu s (%lu min)", (unsigned long)s_metrics.uptime_sec, (unsigned long)(s_metrics.uptime_sec / 60));
        ESP_LOGI(TAG, " Free Heap       : %lu bytes (%lu KiB)", (unsigned long)s_metrics.free_heap, (unsigned long)(s_metrics.free_heap / 1024));
        ESP_LOGI(TAG, " Min Free Heap   : %lu bytes (%lu KiB)", (unsigned long)s_metrics.min_free_heap, (unsigned long)(s_metrics.min_free_heap / 1024));
        ESP_LOGI(TAG, " Wi-Fi RSSI      : %d dBm", s_metrics.wifi_rssi);
        ESP_LOGI(TAG, "=========================================================");
    }
}

esp_err_t sys_diagnostics_init(void)
{
    BaseType_t ret = xTaskCreate(sys_diagnostics_task, "sys_diag_task", 3072, NULL, 1, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sys_diagnostics_task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void sys_diagnostics_get_metrics(sys_health_metrics_t *out_metrics)
{
    if (out_metrics) {
        *out_metrics = s_metrics;
    }
}
