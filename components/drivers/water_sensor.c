/**
 * @file water_sensor.c
 * @brief FreeRTOS task that polls the three water level sensor probes,
 *        validates sensor state, debounces the reading, and publishes
 *        level_event_t messages to g_level_change_queue on every transition.
 *
 * Sensor validity rules (vertical probe column):
 *  Water can only fill from the bottom up.  Therefore:
 *
 *  ┌──────────┬──────────┬──────────┬──────────┬───────────────────┐
 *  │ GPIO10   │ GPIO11   │ GPIO23   │ Level    │ Valid?            │
 *  │ (Low)    │ (Medium) │ (Full)   │          │                   │
 *  ├──────────┼──────────┼──────────┼──────────┼───────────────────┤
 *  │ HIGH     │ HIGH     │ HIGH     │ EMPTY    │ ✓ Valid           │
 *  │ LOW      │ HIGH     │ HIGH     │ LOW      │ ✓ Valid           │
 *  │ LOW      │ LOW      │ HIGH     │ MEDIUM   │ ✓ Valid           │
 *  │ LOW      │ LOW      │ LOW      │ FULL     │ ✓ Valid           │
 *  │ HIGH     │ LOW      │ HIGH     │ (any)    │ ✗ FAULT — medium  │
 *  │          │          │          │          │   wet, low dry    │
 *  │ HIGH     │ HIGH     │ LOW      │ (any)    │ ✗ FAULT — full    │
 *  │          │          │          │          │   wet, low dry    │
 *  │ HIGH     │ LOW      │ LOW      │ (any)    │ ✗ FAULT           │
 *  │ LOW      │ HIGH     │ LOW      │ (any)    │ ✗ FAULT           │
 *  └──────────┴──────────┴──────────┴──────────┴───────────────────┘
 *
 * gpio_bitmask encoding:
 *   bit 0 = GPIO10 raw level (1 = HIGH/dry,  0 = LOW/wet)
 *   bit 1 = GPIO11 raw level (1 = HIGH/dry,  0 = LOW/wet)
 *   bit 2 = GPIO23 raw level (1 = HIGH/dry,  0 = LOW/wet)
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "water_sensor.h"
#include "app_events.h"
#include "gpio_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char *TAG = "WATER_SENSOR";

/* =========================================================================
 * Valid bitmask → level lookup table
 *
 * Key   : gpio_bitmask (bits 2:1:0 = FULL:MED:LOW gpio levels, 1=HIGH/dry)
 * Value : water_level_t  (WATER_LEVEL_INVALID for all other combinations)
 *
 * Only 4 of 8 possible combinations are physically valid.
 * ========================================================================= */
typedef struct {
    uint8_t       bitmask; /* Expected bitmask pattern   */
    water_level_t level;   /* Corresponding level enum   */
} sensor_map_entry_t;

static const sensor_map_entry_t SENSOR_MAP[] = {
    { 0b111, WATER_LEVEL_EMPTY  },  /* All dry   → 0%  */
    { 0b110, WATER_LEVEL_LOW    },  /* Low wet   → 22% */
    { 0b100, WATER_LEVEL_MEDIUM },  /* Low+Med   → 61% */
    { 0b000, WATER_LEVEL_FULL   },  /* All wet   → 100%*/
};

#define SENSOR_MAP_SIZE  (sizeof(SENSOR_MAP) / sizeof(SENSOR_MAP[0]))

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * @brief Sample all three sensor GPIOs and pack into a 3-bit bitmask.
 *
 *  bit 0 = GPIO10 level (1 = HIGH/dry)
 *  bit 1 = GPIO11 level (1 = HIGH/dry)
 *  bit 2 = GPIO23 level (1 = HIGH/dry)
 *
 * @return uint8_t bitmask, value 0–7.
 */
static inline uint8_t sample_gpio_bitmask(void)
{
    uint8_t low    = (uint8_t)gpio_get_level(SENSOR_GPIO_LOW);
    uint8_t medium = (uint8_t)gpio_get_level(SENSOR_GPIO_MEDIUM);
    uint8_t full   = (uint8_t)gpio_get_level(SENSOR_GPIO_FULL);
    return (uint8_t)((full << 2) | (medium << 1) | (low << 0));
}

/**
 * @brief Translate a raw bitmask into a water_level_t using the lookup table.
 *
 * @param bitmask  3-bit GPIO bitmask from sample_gpio_bitmask().
 * @return Validated water_level_t, or WATER_LEVEL_INVALID for illegal combos.
 */
static water_level_t bitmask_to_level(uint8_t bitmask)
{
    for (size_t i = 0; i < SENSOR_MAP_SIZE; i++) {
        if (SENSOR_MAP[i].bitmask == bitmask) {
            return SENSOR_MAP[i].level;
        }
    }
    return WATER_LEVEL_INVALID;
}

/**
 * @brief Return a human-readable string for a water_level_t value.
 *
 * @param level  Water level enum value.
 * @return Constant string literal.
 */
static const char *level_to_string(water_level_t level)
{
    switch (level) {
        case WATER_LEVEL_EMPTY:   return "Empty (0%)";
        case WATER_LEVEL_LOW:     return "Low (~22%)";
        case WATER_LEVEL_MEDIUM:  return "Medium (~61%)";
        case WATER_LEVEL_FULL:    return "Full (100%)";
        case WATER_LEVEL_INVALID: return "SENSOR FAULT";
        default:                  return "Unknown";
    }
}

/**
 * @brief Log the structured serial output required by the specification.
 *        Only called when the level changes.
 *
 * @param bitmask  Raw GPIO bitmask at time of change.
 * @param level    Validated water level.
 */
static void print_level_report(uint8_t bitmask, water_level_t level)
{
    /* Decode individual GPIO states from bitmask for readability */
    const char *gpio10 = (bitmask & 0b001) ? "HIGH" : "LOW";
    const char *gpio11 = (bitmask & 0b010) ? "HIGH" : "LOW";
    const char *gpio23 = (bitmask & 0b100) ? "HIGH" : "LOW";

    const char *level_name;
    const char *pct;

    switch (level) {
        case WATER_LEVEL_EMPTY:
            level_name = "Empty";   pct = "0%";   break;
        case WATER_LEVEL_LOW:
            level_name = "Low";     pct = "~22%";  break;
        case WATER_LEVEL_MEDIUM:
            level_name = "Medium";  pct = "~61%";  break;
        case WATER_LEVEL_FULL:
            level_name = "Full";    pct = "100%";  break;
        default:
            level_name = "FAULT";   pct = "N/A";   break;
    }

    /* Print the structured block as specified */
    printf("\n---------------------------------\n");
    printf("GPIO10 %s\n", gpio10);
    printf("GPIO11 %s\n", gpio11);
    printf("GPIO23 %s\n", gpio23);
    printf("Water Level   : %s\n", level_name);
    printf("Estimated Water: %s\n", pct);
    printf("---------------------------------\n\n");
}

/* =========================================================================
 * FreeRTOS Task Implementation
 * ========================================================================= */

/**
 * @brief Water sensor polling task.
 *
 * Execution flow:
 *  1. Wait for EVT_GPIO_READY (ensures GPIOs are initialized).
 *  2. Poll GPIOs every WATER_SENSOR_POLL_MS.
 *  3. Debounce: accumulate WATER_SENSOR_DEBOUNCE_COUNT consecutive identical
 *     readings before accepting the new level.
 *  4. On confirmed level change: log, print serial report, post to queue.
 *  5. On WATER_LEVEL_INVALID: set EVT_SENSOR_FAULT, log error, skip queue post.
 *
 * @param pvParameters  Unused (required by FreeRTOS task signature).
 */
static void water_sensor_task(void *pvParameters)
{
    (void)pvParameters; /* Suppress unused-parameter warning */

    ESP_LOGI(TAG, "Task started — waiting for GPIO_READY signal...");

    /* Gate on GPIO initialization completing */
    xEventGroupWaitBits(
        g_system_event_group,
        EVT_GPIO_READY,
        pdFALSE,        /* Do not clear the bit */
        pdTRUE,         /* Wait for ALL listed bits */
        portMAX_DELAY   /* Block indefinitely until ready */
    );

    ESP_LOGI(TAG, "GPIO_READY received — beginning sensor polling");
    ESP_LOGI(TAG, "Poll interval: %u ms, debounce count: %u",
             WATER_SENSOR_POLL_MS, WATER_SENSOR_DEBOUNCE_COUNT);

    /* Subscribe water_sensor_task to Task Watchdog Timer (TWDT) */
    esp_task_wdt_add(NULL);

    /* Initialize state tracking */
    water_level_t confirmed_level  = WATER_LEVEL_INVALID; /* Force initial publish */
    water_level_t candidate_level  = WATER_LEVEL_INVALID;
    uint8_t       debounce_counter = 0;
    uint8_t       last_bitmask     = 0xFF;                /* Sentinel: impossible */

    while (1) {
        /* Reset Task Watchdog Timer on every poll cycle */
        esp_task_wdt_reset();

        /* --- Sample GPIOs ------------------------------------------------ */
        uint8_t bitmask = sample_gpio_bitmask();
        water_level_t raw_level = bitmask_to_level(bitmask);

        /* --- Handle invalid sensor state --------------------------------- */
        if (raw_level == WATER_LEVEL_INVALID) {
            /* Only log the fault message once per invalid event, not every poll */
            if (bitmask != last_bitmask) {
                ESP_LOGW(TAG,
                    "SENSOR FAULT — impossible GPIO combination detected "
                    "(GPIO10=%s GPIO11=%s GPIO23=%s bitmask=0b%03u). "
                    "Check probe wiring.",
                    (bitmask & 0b001) ? "HIGH" : "LOW",
                    (bitmask & 0b010) ? "HIGH" : "LOW",
                    (bitmask & 0b100) ? "HIGH" : "LOW",
                    bitmask);
                xEventGroupSetBits(g_system_event_group, EVT_SENSOR_FAULT);
                last_bitmask = bitmask;
            }
            /* Reset debounce so we don't carry stale candidate state */
            debounce_counter = 0;
            candidate_level  = WATER_LEVEL_INVALID;
            vTaskDelay(pdMS_TO_TICKS(WATER_SENSOR_POLL_MS));
            continue;
        }

        /* --- Debounce logic ---------------------------------------------- */
        if (raw_level == candidate_level) {
            debounce_counter++;
        } else {
            /* New candidate — restart counter */
            candidate_level  = raw_level;
            debounce_counter = 1;
        }

        /* --- Confirm level after debounce threshold ---------------------- */
        if (debounce_counter >= WATER_SENSOR_DEBOUNCE_COUNT) {
            debounce_counter = 0; /* Reset so we don't keep re-triggering */

            if (candidate_level != confirmed_level) {
                /* Level has genuinely changed — update tracking state */
                confirmed_level = candidate_level;
                last_bitmask    = bitmask;

                /* Update shared snapshot (read by blynk_task on connect) */
                g_current_level = confirmed_level;

                ESP_LOGI(TAG, "Water level changed -> %s (bitmask=0b%03u)",
                         level_to_string(confirmed_level), bitmask);

                /* Print structured serial report */
                print_level_report(bitmask, confirmed_level);

                /* Build and post the event message */
                level_event_t event = {
                    .level        = confirmed_level,
                    .gpio_bitmask = bitmask,
                };

                if (xQueueSend(g_level_change_queue, &event, 0) != pdTRUE) {
                    /* Queue full — the audio task is busy; log and continue.
                     * We do not block here to avoid stalling the sensor task. */
                    ESP_LOGW(TAG, "Level change queue full — event dropped "
                                  "(audio task may be busy)");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(WATER_SENSOR_POLL_MS));
    }
    /* Tasks must never return — FreeRTOS will assert if they do */
    vTaskDelete(NULL);
}

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

esp_err_t water_sensor_task_start(void)
{
    BaseType_t result = xTaskCreate(
        water_sensor_task,          /* Task function                  */
        "water_sensor",             /* Debug name shown in top        */
        WATER_SENSOR_TASK_STACK,    /* Stack depth in words (bytes)   */
        NULL,                       /* No parameter passed            */
        WATER_SENSOR_TASK_PRIORITY, /* Priority                       */
        NULL                        /* No handle needed externally    */
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed for water_sensor_task "
                      "(stack=%u, priority=%u) — check heap",
                 WATER_SENSOR_TASK_STACK, WATER_SENSOR_TASK_PRIORITY);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "water_sensor_task created (stack=%u bytes, priority=%u)",
             WATER_SENSOR_TASK_STACK, WATER_SENSOR_TASK_PRIORITY);
    return ESP_OK;
}
