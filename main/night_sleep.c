/**
 * @file night_sleep.c
 * @brief Scheduled Nighttime Deep Sleep (11:00 PM – 4:00 AM IST) & Daytime Active Mode.
 *
 * Checks time in India Standard Time (IST UTC+5:30). During 23:00 to 04:00,
 * calculates seconds until 04:00 AM IST, configures RTC Timer & GPIO wakeups,
 * and enters Deep Sleep.
 */

#include "night_sleep.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static const char *TAG = "NIGHT_SLEEP";

void night_sleep_init(void) {
  /* Set timezone to India Standard Time (UTC + 5:30) */
  setenv("TZ", "IST-5:30", 1);
  tzset();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  switch (cause) {
  case ESP_SLEEP_WAKEUP_TIMER:
    ESP_LOGI(TAG, "Boot cause: Woke up from scheduled nighttime sleep (04:00 AM IST)!");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    ESP_LOGI(TAG, "Boot cause: Emergency probe wakeup during nighttime deep sleep!");
    break;
  default:
    ESP_LOGI(TAG, "Boot cause: Normal power-on / hardware reset");
    break;
  }
}

void night_sleep_check_and_enter(void) {
  time_t now = time(NULL);
  if (now < 1700000000) {
    /* Time not yet synchronized via SNTP */
    return;
  }

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  int hour = timeinfo.tm_hour;
  int min = timeinfo.tm_min;
  int sec = timeinfo.tm_sec;

  /* Night rest window: 11:00 PM (23:00) to 04:00 AM */
  if (hour >= 23 || hour < 4) {
    int seconds_until_4am = 0;
    if (hour >= 23) {
      seconds_until_4am = (24 - hour + 4) * 3600 - (min * 60 + sec);
    } else {
      seconds_until_4am = (4 - hour) * 3600 - (min * 60 + sec);
    }

    if (seconds_until_4am < 10) {
      seconds_until_4am = 60; /* Minimum fallback */
    }

    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, " Night Rest Hours (11 PM - 4 AM IST) Active! ");
    ESP_LOGI(TAG, " Current Time: %02d:%02d:%02d IST", hour, min, sec);
    ESP_LOGI(TAG, " Entering Deep Sleep for %d seconds (until 04:00 AM IST)", seconds_until_4am);
    ESP_LOGI(TAG, "=========================================================");

    /* Flush serial buffer before sleep */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Enable 4:00 AM timer wakeup */
    esp_sleep_enable_timer_wakeup((uint64_t)seconds_until_4am * 1000000ULL);

    /* Enter Deep Sleep mode (< 10 uA power) */
    esp_deep_sleep_start();
  }
}

static void night_monitor_task(void *arg) {
  (void)arg;
  /* Wait 15 seconds after boot for SNTP time sync to complete */
  vTaskDelay(pdMS_TO_TICKS(15000));

  for (;;) {
    night_sleep_check_and_enter();
    /* Check every 60 seconds */
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void night_sleep_start_monitor_task(void) {
  xTaskCreate(night_monitor_task, "night_monitor", 3072, NULL, 1, NULL);
}
