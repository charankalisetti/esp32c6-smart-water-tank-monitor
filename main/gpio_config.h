/**
 * @file gpio_config.h
 * @brief GPIO pin assignments and initialization interface for the
 *        Water Tank Level Monitor (ESP32-C6 DevKitC-1 v1.2).
 *
 * Water Sensor Probe Wiring
 * ─────────────────────────
 *  Common Probe → GND (fixed reference, 3 cm height)
 *  Low Probe    → GPIO10 (20 cm)
 *  Medium Probe → GPIO11 (55 cm)
 *  Full Probe   → GPIO23 (90 cm)
 *
 * Sensor Logic
 * ─────────────
 *  DRY  → GPIO reads HIGH (internal pull-up holds line high)
 *  WET  → GPIO reads LOW  (water conducts, pulls line to GND via common probe)
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Water Sensor GPIO Pin Definitions
 * ========================================================================= */

/** GPIO connected to the Low water level probe (20 cm from tank base). */
#define SENSOR_GPIO_LOW     GPIO_NUM_10

/** GPIO connected to the Medium water level probe (55 cm from tank base). */
#define SENSOR_GPIO_MEDIUM  GPIO_NUM_11

/** GPIO connected to the Full water level probe (90 cm from tank base). */
#define SENSOR_GPIO_FULL    GPIO_NUM_23

/* =========================================================================
 * I2S GPIO Pin Definitions (MAX98357A)
 * ========================================================================= */

/** I2S Bit Clock — MAX98357A BCLK pin. */
#define I2S_GPIO_BCLK   GPIO_NUM_19

/** I2S Word Select (LRC) — MAX98357A LRC pin. */
#define I2S_GPIO_LRC    GPIO_NUM_18

/** I2S Data Out — MAX98357A DIN pin. */
#define I2S_GPIO_DOUT   GPIO_NUM_20

/* =========================================================================
 * 12V Active Buzzer Transistor Switch GPIO Pin Definition
 * ========================================================================= */

/** GPIO connected to PN2222A NPN Transistor Base via 1k resistor. */
#define BUZZER_GPIO     GPIO_NUM_21

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Initialize all water sensor GPIO pins.
 *
 * Configures GPIO10, GPIO11, and GPIO23 as:
 *  - Direction : INPUT
 *  - Pull-up   : ENABLED  (line is HIGH when probe is dry)
 *  - Pull-down : DISABLED
 *  - Interrupt : DISABLED (level is polled, not interrupt-driven)
 *
 * I2S GPIO pins are NOT configured here — the I2S driver configures its
 * own pins during i2s_channel_init_std_mode().
 *
 * @return ESP_OK on success, or an esp_err_t code on failure.
 */
esp_err_t gpio_sensors_init(void);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_CONFIG_H */
