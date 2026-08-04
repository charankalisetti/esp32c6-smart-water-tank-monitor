/**
 * @file wifi_config.h
 * @brief Wi-Fi and Blynk cloud credentials.
 *
 * !! KEEP THIS FILE PRIVATE — never commit to version control !!
 * Add to .gitignore:  main/wifi_config.h
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

/* ---------------------------------------------------------------------------
 * Wi-Fi Station credentials
 * --------------------------------------------------------------------------- */
#define WIFI_SSID           "railwirefibernet"
#define WIFI_PASSWORD       "Charan@1904"

/** Maximum number of reconnection attempts before giving up (0 = unlimited). */
#define WIFI_MAX_RETRIES    0u

/* ---------------------------------------------------------------------------
 * Blynk IoT cloud settings
 * --------------------------------------------------------------------------- */
#define BLYNK_TEMPLATE_ID   "TMPL3Ek9-WKI9"
#define BLYNK_TEMPLATE_NAME "Water Tank Monitor"
#define BLYNK_AUTH_TOKEN    "nj2HmnaCm4oW-6DU_ezR-v8kpiz12Gto"
#define BLYNK_SERVER        "blynk.cloud"
#define BLYNK_PORT          443          /* HTTPS */

/* ---------------------------------------------------------------------------
 * Blynk Virtual Pin assignments
 *
 *  V0  — Water status string  ("Tank Empty", "Low", "Medium", "Full")
 *  V1  — Water percentage     (integer: 0, 22, 61, 100)
 *  V2  — Low probe state      (1 = wet/LOW, 0 = dry/HIGH)
 *  V3  — Medium probe state   (1 = wet/LOW, 0 = dry/HIGH)
 *  V4  — Full probe state     (1 = wet/LOW, 0 = dry/HIGH)
 * --------------------------------------------------------------------------- */
#define BLYNK_PIN_STATUS    "V0"
#define BLYNK_PIN_PERCENT   "V1"
#define BLYNK_PIN_GPIO10    "V2"
#define BLYNK_PIN_GPIO11    "V3"
#define BLYNK_PIN_GPIO23    "V4"

/** Blynk event name used for push notifications (create in Blynk dashboard). */
#define BLYNK_EVENT_LEVEL   "level_change"

/* ---------------------------------------------------------------------------
 * Sinric Pro (Google Home / Google Assistant) Credentials
 * Obtain free credentials at: https://sinric.pro
 * --------------------------------------------------------------------------- */
#define SINRIC_PRO_APP_KEY    "YOUR_SINRIC_PRO_APP_KEY"
#define SINRIC_PRO_APP_SECRET "YOUR_SINRIC_PRO_APP_SECRET"
#define SINRIC_PRO_DEVICE_ID  "YOUR_SINRIC_PRO_DEVICE_ID"

#endif /* WIFI_CONFIG_H */
