/**
 * @file buzzer.h
 * @brief Non-blocking 12V Active Buzzer driver for ESP32-C6 (GPIO21).
 *
 * Controls a 12V active buzzer via a PN2222A NPN transistor switch.
 * Uses a dedicated FreeRTOS worker task and queue to execute alarm patterns
 * non-blockingly without stalling sensor polling or network tasks.
 *
 * Alarm Patterns:
 *  - Tank Empty  : Continuous 5 seconds ON
 *  - Water Low   : 2 Beeps (300 ms ON, 300 ms OFF)
 *  - Medium      : Voice Only (No Buzzer)
 *  - Tank Full   : 1 Confirmation Beep (500 ms ON)
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef BUZZER_H
#define BUZZER_H

#include "esp_err.h"
#include "gpio_config.h"
#include "app_events.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** GPIO pin assigned to buzzer NPN transistor base (GPIO21). */
#ifndef BUZZER_GPIO
#define BUZZER_GPIO GPIO_NUM_21
#endif

/**
 * @brief Initialize GPIO21 for buzzer control and spawn the non-blocking worker task.
 *
 * Configures GPIO21 as OUTPUT, initially LOW (buzzer OFF).
 * Spawns FreeRTOS `buzzer_task` with queue depth 4.
 */
void buzzer_init(void);

/**
 * @brief Turn the buzzer ON immediately (GPIO21 HIGH).
 */
void buzzer_on(void);

/**
 * @brief Turn the buzzer OFF immediately (GPIO21 LOW).
 */
void buzzer_off(void);

/**
 * @brief Asynchronously request a beep pattern.
 *
 * @param on_ms   Duration in milliseconds to turn buzzer ON per cycle.
 * @param off_ms  Duration in milliseconds to turn buzzer OFF per cycle.
 * @param count   Number of beep cycles to repeat.
 */
void buzzer_beep(uint32_t on_ms, uint32_t off_ms, uint8_t count);

/**
 * @brief Trigger the level-specific alarm pattern after voice announcements finish.
 *
 * @param level Validated water level enum (EMPTY, LOW, MEDIUM, FULL).
 */
void buzzer_play_pattern(water_level_t level);

/**
 * @brief Check if the buzzer worker task is currently executing a pattern.
 *
 * @return true if a pattern is active, false if idle.
 */
bool buzzer_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
