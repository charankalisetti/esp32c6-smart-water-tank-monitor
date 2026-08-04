#ifndef WIFI_PROV_H
#define WIFI_PROV_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start Wi-Fi provisioning via BLE (Bluetooth Low Energy).
 *
 * Checks NVS Flash to see if valid Wi-Fi credentials exist.
 * If credentials exist, connects directly to Wi-Fi.
 * If credentials do NOT exist (or if forced via reset), turns on Bluetooth LE
 * and advertises device name "Water-Monitor-Setup".
 *
 * @param force_reprovision If true, erases NVS Wi-Fi credentials and forces BLE provisioning mode.
 * @return ESP_OK on success.
 */
esp_err_t wifi_prov_init(bool force_reprovision);

/**
 * @brief Erase stored Wi-Fi credentials in NVS Flash.
 */
esp_err_t wifi_prov_reset_credentials(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_PROV_H
