/**
 * @file gpio_config.c
 * @brief GPIO initialization for the three water level sensor probes.
 *
 * Configures GPIO10, GPIO11, GPIO23 as digital inputs with internal
 * pull-ups enabled.  When a probe is submerged, the common GND probe
 * conducts water to the sense probe, pulling the GPIO low.
 *
 * The I2S GPIOs (18, 19, 20) are intentionally excluded here — they are
 * configured automatically by the ESP-IDF I2S standard driver when
 * i2s_channel_init_std_mode() is called in audio_player.c.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "gpio_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/gpio.h"

static const char *TAG = "GPIO_CFG";

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/**
 * @brief Configure a single GPIO as input with NO pull-up (floating).
 *
 * Anti-corrosion design: GPIOs start floating (zero current through water).
 * The water_sensor_task enables pull-ups for a brief 2ms pulse during each
 * sample, then disables them again. This reduces electrolysis corrosion
 * by ~2500x compared to always-on pull-ups.
 *
 * @param pin  GPIO number to configure.
 * @return ESP_OK on success.
 */
static esp_err_t configure_sensor_pin(gpio_num_t pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),         /* Select this specific pin      */
        .mode         = GPIO_MODE_INPUT,        /* Input only — MCU never drives */
        .pull_up_en   = GPIO_PULLUP_DISABLE,    /* OFF by default (anti-corrosion) */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  /* No competing pull-down        */
        .intr_type    = GPIO_INTR_DISABLE,      /* Polled — no ISR needed        */
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config() failed for GPIO%d: %s", pin, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "GPIO%d configured — INPUT, FLOATING (anti-corrosion mode)", pin);
    return ESP_OK;
}

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

esp_err_t gpio_sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing water sensor GPIO pins...");

    /* Configure Low probe (GPIO10) */
    ESP_RETURN_ON_ERROR(
        configure_sensor_pin(SENSOR_GPIO_LOW),
        TAG, "Failed to configure LOW sensor GPIO%d", SENSOR_GPIO_LOW
    );

    /* Configure Medium probe (GPIO11) */
    ESP_RETURN_ON_ERROR(
        configure_sensor_pin(SENSOR_GPIO_MEDIUM),
        TAG, "Failed to configure MEDIUM sensor GPIO%d", SENSOR_GPIO_MEDIUM
    );

    /* Configure Full probe (GPIO22) */
    ESP_RETURN_ON_ERROR(
        configure_sensor_pin(SENSOR_GPIO_FULL),
        TAG, "Failed to configure FULL sensor GPIO%d", SENSOR_GPIO_FULL
    );

    ESP_LOGI(TAG, "All sensor GPIOs initialized successfully "
                  "(Low=GPIO%d, Medium=GPIO%d, Full=GPIO%d)",
             SENSOR_GPIO_LOW, SENSOR_GPIO_MEDIUM, SENSOR_GPIO_FULL);

    return ESP_OK;
}
