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
#define WATER_SENSOR_TASK_STACK   4096u

/** FreeRTOS priority for the sensor task (low — it only polls GPIOs). */
#define WATER_SENSOR_TASK_PRIORITY  2u

/** GPIO poll interval in milliseconds.
 *  5000 ms (5 seconds) between pulsed strobe samples.
 *  Anti-corrosion: pull-ups are only ON for 2ms every 5 seconds (0.04% duty).
 *  Water level changes over minutes — 5 second detection is instant enough. */
#define WATER_SENSOR_POLL_MS        5000u

/** Duration in milliseconds to enable pull-ups before sampling GPIOs.
 *  2 ms is sufficient for the internal pull-up (~45kΩ) to charge the GPIO
 *  capacitance and stabilize the digital reading. */
#define WATER_SENSOR_STROBE_MS      2u

/**
 * @brief Number of consecutive identical readings required to accept a
 *        new level as valid (debounce).
 *
 *  5000 ms × 3 = 15000 ms (15 seconds) debounce window.
 *  Eliminates false triggers from water surface ripple at probe threshold.
 */
#define WATER_SENSOR_DEBOUNCE_COUNT  3u

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
