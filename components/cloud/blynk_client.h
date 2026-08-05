/**
 * @file blynk_client.h
 * @brief Blynk IoT cloud client for the Water Tank Level Monitor.
 *
 * Provides a FreeRTOS task that:
 *  - Waits for EVT_WIFI_CONNECTED before any network operation
 *  - Sends HTTPS updates to Blynk cloud when the water level changes
 *  - Sends a Blynk push notification on every level transition
 *
 * Virtual pin mapping (defined in wifi_config.h):
 *   V0 — Status label string
 *   V1 — Water percentage (0/22/61/100)
 *   V2 — GPIO10 low probe  (1=wet, 0=dry)
 *   V3 — GPIO11 med probe  (1=wet, 0=dry)
 *   V4 — GPIO23 full probe (1=wet, 0=dry)
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef BLYNK_CLIENT_H
#define BLYNK_CLIENT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create and start the Blynk client FreeRTOS task.
 *
 * The task blocks on EVT_WIFI_CONNECTED before attempting any HTTPS
 * connection, so this may be called before Wi-Fi is up.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if task creation fails.
 */
esp_err_t blynk_task_start(void);

#ifdef __cplusplus
}
#endif

#endif /* BLYNK_CLIENT_H */
