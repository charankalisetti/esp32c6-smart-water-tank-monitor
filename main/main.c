/**
 * @file main.c
 * @brief ESP-IDF application entry point.
 *
 * This file is intentionally thin — its sole responsibility is to call
 * app_main_run() and assert success.  All initialization logic lives in
 * app_main.c to keep this file free of implementation detail.
 *
 * After app_main_run() returns:
 *  - The FreeRTOS scheduler is running.
 *  - water_sensor_task and audio_player_task are active.
 *  - This function sleeps in a low-priority idle loop (no busy waiting).
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "app_main.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    /* Delegate all initialization and task creation to the orchestrator */
    esp_err_t ret = app_main_run();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed: %s — restarting in 5 s",
                 esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    /*
     * Cleanly delete the app_main task to reclaim its stack RAM (~3 KB).
     * Background FreeRTOS tasks (water_sensor, audio_player, blynk, sinric,
     * night_sleep) continue executing independently.
     */
    ESP_LOGI(TAG, "Initialization complete — deleting main task to reclaim RAM");
    vTaskDelete(NULL);
}
