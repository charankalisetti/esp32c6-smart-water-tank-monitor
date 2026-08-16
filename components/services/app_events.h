/**
 * @file app_events.h
 * @brief Application-wide event definitions, dedicated subscriber queues,
 *        and inter-task message types for the Water Tank Level Monitor.
 *
 * Dedicated Queue Publisher Pattern:
 *   water_sensor_task  --> [g_audio_queue]   --> audio_player_task
 *                      --> [g_blynk_queue]   --> blynk_task
 *                      --> [g_sinric_queue]  --> sinric_task
 *
 * @author Principal Embedded Systems Engineer
 */

#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATER_LEVEL_EMPTY          = 0,  /**< All sensors dry:          GPIO10 H, GPIO11 H, GPIO22 H */
    WATER_LEVEL_LOW            = 1,  /**< Low sensor wet only:      GPIO10 L, GPIO11 H, GPIO22 H */
    WATER_LEVEL_MEDIUM         = 2,  /**< Low+medium wet:           GPIO10 L, GPIO11 L, GPIO22 H */
    WATER_LEVEL_FULL           = 3,  /**< All three sensors wet:    GPIO10 L, GPIO11 L, GPIO22 L */
    WATER_LEVEL_FAULT_LOW      = 4,  /**< Low probe (GPIO10, 20cm) open-circuit / corroded */
    WATER_LEVEL_FAULT_MED      = 5,  /**< Med probe (GPIO11, 55cm) open-circuit / corroded */
    WATER_LEVEL_FAULT_GENERAL  = 6,  /**< Multiple probe open or Full probe shorted to GND */
    WATER_LEVEL_INVALID        = 7,  /**< Unclassified hardware anomaly */
} water_level_t;

typedef struct {
    water_level_t level;        /**< Validated water level enum value */
    uint8_t       gpio_bitmask; /**< Raw GPIO state snapshot at time of change */
} level_event_t;

/* System Event Group Bits */
#define EVT_I2S_READY          (1 << 0)
#define EVT_GPIO_READY         (1 << 1)
#define EVT_SENSOR_FAULT       (1 << 2)
#define EVT_AUDIO_PLAYING      (1 << 3)
#define EVT_BOOT_COMPLETE      (1 << 4)
#define EVT_WIFI_CONNECTED     (1 << 5)

/* Dedicated Subscriber Queues (prevents FreeRTOS queue contention/stealing) */
extern QueueHandle_t g_audio_queue;
extern QueueHandle_t g_blynk_queue;
extern QueueHandle_t g_sinric_queue;

extern EventGroupHandle_t g_system_event_group;
extern SemaphoreHandle_t  g_tls_handshake_mutex;
extern volatile water_level_t g_current_level;

#define APP_LEVEL_QUEUE_DEPTH   4u

/**
 * @brief Create and initialize application queues and event group.
 */
esp_err_t app_events_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENTS_H */
