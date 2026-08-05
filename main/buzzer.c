/**
 * @file buzzer.c
 * @brief Implementation of the non-blocking 12V Active Buzzer driver (GPIO21).
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "buzzer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";

/** Pattern request message structure. */
typedef struct {
  uint32_t on_ms;
  uint32_t off_ms;
  uint8_t count;
} buzzer_cmd_t;

static QueueHandle_t s_buzzer_queue = NULL;
static bool s_is_busy = false;
static bool s_initialized = false;

void buzzer_on(void) {
  gpio_set_level(BUZZER_GPIO, 1);
  ESP_LOGI(TAG, "Buzzer ON (GPIO21 HIGH)");
}

void buzzer_off(void) {
  gpio_set_level(BUZZER_GPIO, 0);
  ESP_LOGI(TAG, "Buzzer OFF (GPIO21 LOW)");
}

bool buzzer_is_busy(void) { return s_is_busy; }

/**
 * @brief FreeRTOS worker task executing buzzer patterns asynchronously.
 */
static void buzzer_task(void *pvParameters) {
  (void)pvParameters;
  buzzer_cmd_t cmd;

  for (;;) {
    if (xQueueReceive(s_buzzer_queue, &cmd, portMAX_DELAY) == pdTRUE) {
      s_is_busy = true;
      ESP_LOGI(TAG, "Pattern Started (ON=%lu ms, OFF=%lu ms, Count=%u)",
               (unsigned long)cmd.on_ms, (unsigned long)cmd.off_ms,
               (unsigned int)cmd.count);

      for (uint8_t i = 0; i < cmd.count; i++) {
        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(cmd.on_ms));

        buzzer_off();
        if (i < cmd.count - 1 && cmd.off_ms > 0) {
          vTaskDelay(pdMS_TO_TICKS(cmd.off_ms));
        }
      }

      ESP_LOGI(TAG, "Pattern Finished");
      s_is_busy = false;
    }
  }
}

void buzzer_init(void) {
  if (s_initialized) {
    ESP_LOGW(TAG, "Buzzer already initialized — skipping");
    return;
  }

  /* Configure GPIO21 as OUTPUT, initialized LOW */
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << BUZZER_GPIO),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  esp_err_t err = gpio_config(&io_conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure GPIO21 for buzzer: %s",
             esp_err_to_name(err));
    return;
  }

  gpio_set_level(BUZZER_GPIO, 0);

  /* Create request queue and worker task */
  s_buzzer_queue = xQueueCreate(4, sizeof(buzzer_cmd_t));
  if (!s_buzzer_queue) {
    ESP_LOGE(TAG, "Failed to create buzzer queue");
    return;
  }

  BaseType_t res = xTaskCreate(buzzer_task, "buzzer_task", 2048, NULL, 2, NULL);
  if (res != pdPASS) {
    ESP_LOGE(TAG, "Failed to create buzzer task");
    return;
  }

  s_initialized = true;
  ESP_LOGI(TAG, "Buzzer Initialized (GPIO21 OUTPUT, default LOW)");
}

void buzzer_beep(uint32_t on_ms, uint32_t off_ms, uint8_t count) {
  if (!s_initialized) {
    ESP_LOGE(TAG, "Buzzer not initialized!");
    return;
  }

  if (s_is_busy) {
    ESP_LOGW(TAG, "Pattern already running — dropping duplicate request");
    return;
  }

  buzzer_cmd_t cmd = {
      .on_ms = on_ms,
      .off_ms = off_ms,
      .count = count,
  };

  if (xQueueSend(s_buzzer_queue, &cmd, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Buzzer queue full — dropping request");
  }
}

void buzzer_play_pattern(water_level_t level) {
  switch (level) {
  case WATER_LEVEL_EMPTY:
    /* Tank Empty: Continuous buzzer for 5 seconds */
    buzzer_beep(5000, 0, 1);
    break;

  case WATER_LEVEL_LOW:
    /* Water Level Low: 2 Beeps (ON 300ms, OFF 300ms) */
    buzzer_beep(300, 300, 2);
    break;

  case WATER_LEVEL_MEDIUM:
    /* Medium: Continuous buzzer for 3 seconds */
    buzzer_beep(3000, 0, 1);
    break;

  case WATER_LEVEL_FULL:
    /* Tank Full: 1 Confirmation Beep (ON 500ms) */
    buzzer_beep(500, 0, 1);
    break;

  default:
    break;
  }
}
