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
 * --------------------------------------------------------------------------- */
#define SINRIC_PRO_APP_KEY    "708e98d0-88a5-40ab-a3a9-2b8dd1a79c18"
#define SINRIC_PRO_APP_SECRET "5a062f26-b572-4369-b9b7-5a53b5decc5f-3558e9ed-e16f-47c3-babc-aff39d2db945"
#define SINRIC_PRO_DEVICE_ID  "6a7186e509efd1746c350d10"

#endif /* WIFI_CONFIG_H */
