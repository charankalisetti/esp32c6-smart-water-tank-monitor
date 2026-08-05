/**
 * @file wifi_manager.h
 * @brief Wi-Fi Station mode manager for the Water Tank Level Monitor.
 *
 * Connects the ESP32-C6 to a home Wi-Fi router, handles reconnections,
 * and sets EVT_WIFI_CONNECTED in g_system_event_group when an IP is obtained.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Wi-Fi subsystem and begin connecting.
 *
 * Initializes NVS flash, the TCP/IP stack, and the ESP-IDF Wi-Fi driver,
 * then starts a connection attempt to the SSID defined in wifi_config.h.
 *
 * This function returns immediately after starting the connection.
 * The caller should wait for EVT_WIFI_CONNECTED in g_system_event_group
 * before using any network functionality.
 *
 * @return ESP_OK on success, ESP error code on failure.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Return the current IP address as a string.
 *
 * @param buf   Buffer to write into (at least 16 bytes).
 * @param len   Size of buf.
 */
void wifi_manager_get_ip(char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
