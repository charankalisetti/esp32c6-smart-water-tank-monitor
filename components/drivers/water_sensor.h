/**
 * @file water_sensor.h
 * @brief Water level sensor polling task interface.
 *
 * Provides the FreeRTOS task that continuously samples the three GPIO probes,
 * validates the reading, determines the water level state, and posts a
 * level_event_t to g_level_change_queue whenever the level changes.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef WATER_SENSOR_H
#define WATER_SENSOR_H

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Task Configuration
 * ========================================================================= */

/** Stack size (bytes) allocated to the sensor polling task. */
#define WATER_SENSOR_TASK_STACK   2048u

/** FreeRTOS priority for the sensor task (low — it only polls GPIOs). */
#define WATER_SENSOR_TASK_PRIORITY  2u

/** GPIO poll interval in milliseconds.
 *  50 ms gives 20 readings/second — fast enough to catch transitions,
 *  slow enough to avoid unnecessary CPU load and GPIO bounce issues. */
#define WATER_SENSOR_POLL_MS        50u

/**
 * @brief Number of consecutive identical readings required to accept a
 *        new level as valid (debounce).
 *
 *  50 ms × 20 = 1000 ms (1 second) debounce window.
 *  Eliminates false triggers from water surface ripple at probe threshold.
 *  Increase further if oscillation persists (e.g., 40 = 2 seconds).
 */
#define WATER_SENSOR_DEBOUNCE_COUNT  20u

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Create and start the water sensor polling FreeRTOS task.
 *
 * The task gates on EVT_GPIO_READY before beginning sensor reads.
 * On each level change (after debounce), it posts a level_event_t message
 * to g_level_change_queue.
 *
 * Must be called after:
 *  1. app_events_init()
 *  2. gpio_sensors_init()
 *  3. EVT_GPIO_READY has been set in g_system_event_group
 *
 * @return ESP_OK on task creation success.
 *         ESP_FAIL if xTaskCreate() returns pdFALSE.
 */
esp_err_t water_sensor_task_start(void);

#ifdef __cplusplus
}
#endif

#endif /* WATER_SENSOR_H */
