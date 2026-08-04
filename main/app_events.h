/**
 * @file app_events.h
 * @brief Application-wide event definitions, shared queue/event-group handles,
 *        and inter-task message types for the Water Tank Level Monitor.
 *
 * Communication model:
 *   water_sensor_task  --> [level_queue]  --> audio_player_task
 *                      --> [level_queue]  --> blynk_task (cloud push)
 *                      --> [level_queue]  --> app_main_task  (serial print)
 *
 *   g_current_level  : volatile snapshot updated by water_sensor_task;
 *                       read by blynk_task without queue contention.
 *
 *   System-wide signals are broadcast via [system_event_group] bits so any
 *   task can gate on readiness without polling global variables.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Water Level States
 * Ordered by physical sensor position (lowest to highest).
 * ========================================================================= */

/**
 * @brief Canonical water level states produced by the sensor task.
 *
 * WATER_LEVEL_INVALID is used when an impossible GPIO combination is detected
 * (e.g., medium sensor active but low sensor not active — physically impossible
 * in a vertical probe configuration with a common ground probe).
 */
typedef enum {
    WATER_LEVEL_EMPTY   = 0,  /**< All sensors dry:          GPIO10 H, GPIO11 H, GPIO23 H */
    WATER_LEVEL_LOW     = 1,  /**< Low sensor wet only:      GPIO10 L, GPIO11 H, GPIO23 H */
    WATER_LEVEL_MEDIUM  = 2,  /**< Low+medium wet:           GPIO10 L, GPIO11 L, GPIO23 H */
    WATER_LEVEL_FULL    = 3,  /**< All three sensors wet:    GPIO10 L, GPIO11 L, GPIO23 L */
    WATER_LEVEL_INVALID = 4,  /**< Impossible sensor combination — hardware fault */
} water_level_t;

/* =========================================================================
 * Level Change Event — payload sent through the inter-task queue
 * ========================================================================= */

/**
 * @brief Message posted to the level_change_queue when the water level
 *        transitions to a new state.
 *
 * Carries both the new level and the raw GPIO bitmask for diagnostic logging.
 *
 *  gpio_bitmask bit layout:
 *    bit 0 → GPIO10 (Low probe)   — 0 = wet, 1 = dry
 *    bit 1 → GPIO11 (Medium probe) — 0 = wet, 1 = dry
 *    bit 2 → GPIO23 (Full probe)   — 0 = wet, 1 = dry
 */
typedef struct {
    water_level_t level;        /**< Validated water level enum value */
    uint8_t       gpio_bitmask; /**< Raw GPIO state snapshot at time of change */
} level_event_t;

/* =========================================================================
 * System Event Group Bit Definitions
 * ========================================================================= */

/** I2S driver initialized successfully — audio task ready to accept messages. */
#define EVT_I2S_READY          (1 << 0)

/** GPIO driver initialized — sensor task may begin polling. */
#define EVT_GPIO_READY         (1 << 1)

/** A sensor fault (invalid combination) was detected — logged, not fatal. */
#define EVT_SENSOR_FAULT       (1 << 2)

/** Audio playback is currently in progress — prevents re-entrancy. */
#define EVT_AUDIO_PLAYING      (1 << 3)

/** System has fully booted and all tasks are running. */
#define EVT_BOOT_COMPLETE      (1 << 4)

/** Wi-Fi STA has obtained an IP address — network stack is ready. */
#define EVT_WIFI_CONNECTED     (1 << 5)

/* =========================================================================
 * Shared Handle Declarations
 *
 * Defined (created) in app_events.c — extern here so all other modules can
 * access them without passing handles through function arguments.
 * ========================================================================= */

/**
 * @brief Queue carrying level_event_t messages from the sensor task.
 *
 * Producers : water_sensor_task
 * Consumers : audio_player_task, app_main_task (for serial print)
 *
 * Depth      : APP_LEVEL_QUEUE_DEPTH
 * Item size  : sizeof(level_event_t)
 */
extern QueueHandle_t g_level_change_queue;

/**
 * @brief EventGroup for system-wide readiness and status signals.
 *
 * Set by : app_main / audio_player / water_sensor
 * Read by: any task that must gate on system state
 */
extern EventGroupHandle_t g_system_event_group;

/**
 * @brief Current water level — updated atomically by water_sensor_task
 *        on every confirmed level transition.
 *
 * Read by blynk_task to push an initial state on Wi-Fi connect without
 * having to wait for the next sensor change event.
 *
 * Access rule: only water_sensor_task writes; all others read only.
 * A volatile qualifier provides visibility without a mutex for this
 * single-writer / multi-reader pattern on a 32-bit RISC-V core.
 */
extern volatile water_level_t g_current_level;

/* =========================================================================
 * Queue Configuration
 * ========================================================================= */

/** Number of pending level events the queue can buffer. */
#define APP_LEVEL_QUEUE_DEPTH   4u

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Create and initialize the application queue and event group.
 *
 * Must be called from app_main() before any tasks are created.
 *
 * @return ESP_OK on success.
 *         ESP_ERR_NO_MEM if FreeRTOS object allocation fails.
 */
esp_err_t app_events_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENTS_H */
