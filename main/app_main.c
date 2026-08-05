/**
 * @file app_main.c
 * @brief System orchestrator — initializes all subsystems in dependency order
 *        and spawns the FreeRTOS application tasks.
 *
 * Boot sequence:
 *   1. app_events_init()         — Create queue + event group
 *   2. wifi_manager_init()       — Start Wi-Fi STA (async, non-blocking)
 *   3. gpio_sensors_init()       — Configure GPIO10/11/23 with pull-ups
 *   4. xEventGroupSetBits(EVT_GPIO_READY)   — Ungate sensor task polling
 *   5. audio_player_init()       — Initialize I2S + create audio_player_task
 *   6. water_sensor_task_start() — Create sensor polling task
 *   7. blynk_task_start()        — Create Blynk cloud push task
 *   8. xEventGroupSetBits(EVT_BOOT_COMPLETE) — Signal full system ready
 *
 * After step 8, app_main_run() returns and the FreeRTOS scheduler takes over.
 * Wi-Fi connection completes asynchronously; blynk_task gates on
 * EVT_WIFI_CONNECTED before making any network calls.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "app_main.h"
#include "app_events.h"
#include "gpio_config.h"
#include "water_sensor.h"
#include "audio_player.h"
#include "wifi_manager.h"
#include "blynk_client.h"
#include "sinric_client.h"
#include "night_sleep.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "APP_MAIN";

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

esp_err_t app_main_run(void)
{
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, " IoT Water Tank Level Monitor — Booting");
    ESP_LOGI(TAG, " Board : ESP32-C6 DevKitC-1 v1.2");
    ESP_LOGI(TAG, " Amp   : MAX98357A (GPIO18/19/20)");
    ESP_LOGI(TAG, " Probes: Low=GPIO10, Med=GPIO11, Full=GPIO23");
    ESP_LOGI(TAG, " Cloud : Blynk IoT (blynk.cloud)");
    ESP_LOGI(TAG, "============================================");

    esp_err_t ret;

    /* Reconfigure Task Watchdog Timer (TWDT) for 15-second system recovery */
    esp_task_wdt_config_t twdt_cfg = {
        .timeout_ms = 15000,
        .idle_core_mask = (1 << 0),
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&twdt_cfg);
    ESP_LOGI(TAG, "Task Watchdog Timer (TWDT) reconfigured (timeout=15 s, panic=true)");

    /* ------------------------------------------------------------------
     * Step 1: Create shared IPC objects (queue + event group)
     * All other modules depend on these — must be first.
     * ------------------------------------------------------------------ */
    ret = app_events_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "app_events_init() failed: %s — system cannot boot",
                 esp_err_to_name(ret));
        return ret;
    }

    /* ------------------------------------------------------------------
     * Step 2: Initialize Wi-Fi (non-blocking — connection is async)
     * blynk_task gates on EVT_WIFI_CONNECTED, so this just starts the
     * connection process and returns immediately.
     * ------------------------------------------------------------------ */
    ret = wifi_manager_init();
    if (ret != ESP_OK) {
        /* Wi-Fi failure is non-fatal — system still works locally */
        ESP_LOGW(TAG, "wifi_manager_init() failed: %s — running without cloud",
                 esp_err_to_name(ret));
    }

    /* ------------------------------------------------------------------
     * Step 3: Initialize water sensor GPIO pins
     * ------------------------------------------------------------------ */
    ret = gpio_sensors_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_sensors_init() failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "GPIO initialized — sensor probes ready");

    /* Signal to the sensor task that GPIOs are ready for polling */
    xEventGroupSetBits(g_system_event_group, EVT_GPIO_READY);
    ESP_LOGI(TAG, "EVT_GPIO_READY set");

    /* ------------------------------------------------------------------
     * Step 4: Initialize I2S audio driver and start audio player task
     * audio_player_init() sets EVT_I2S_READY internally on success.
     * ------------------------------------------------------------------ */
    ret = audio_player_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_init() failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ------------------------------------------------------------------
     * Step 5: Start water sensor polling task
     * The task gates on EVT_GPIO_READY (already set above) before polling.
     * ------------------------------------------------------------------ */
    ret = water_sensor_task_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "water_sensor_task_start() failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ------------------------------------------------------------------
     * Step 6: Start Blynk cloud push task and Sinric Pro Google Home task
     * Tasks block internally on EVT_WIFI_CONNECTED — safe to start now.
     * ------------------------------------------------------------------ */
    ret = blynk_task_start();
    if (ret != ESP_OK) {
        /* Non-fatal — local operation continues without cloud */
        ESP_LOGW(TAG, "blynk_task_start() failed: %s — no cloud updates",
                 esp_err_to_name(ret));
    }

    ret = sinric_client_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "sinric_client_init() failed: %s — no Sinric Pro updates",
                 esp_err_to_name(ret));
    }

    /* Initialize Nighttime Deep Sleep & Timezone */
    night_sleep_init();
    night_sleep_start_monitor_task();

    /* ------------------------------------------------------------------
     * Step 7: Signal full boot completion
     * ------------------------------------------------------------------ */
    xEventGroupSetBits(g_system_event_group, EVT_BOOT_COMPLETE);
    ESP_LOGI(TAG, "Boot complete — scheduler now owns execution");
    ESP_LOGI(TAG, "Tasks running: water_sensor | audio_player | blynk | sinric | night_monitor");

    return ESP_OK;
}

