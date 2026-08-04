/**
 * @file sinric_client.c
 * @brief Sinric Pro Google Home integration via WebSocket over esp_tls.
 *
 * Implements a minimal RFC-6455 WebSocket client on top of esp_tls so
 * no external library is required.  The handshake sends the Sinric Pro
 * authentication headers (appkey, deviceids, platform, version) and then
 * sends JSON "sendTemperatureEvent" frames on level change + 20 s heartbeat.
 *
 * TLS root: ISRG Root YR + Let's Encrypt YR1 intermediate
 *   (ws.sinric.pro uses Let's Encrypt, NOT Google Trust Services)
 */

#include "sinric_client.h"
#include "wifi_config.h"
#include "app_events.h"
#include "water_sensor.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_random.h"

static const char *TAG = "SINRIC_PRO";

/* ----------------------------------------------------------------------- */
/* TLS trust chain for ws.sinric.pro (verified Aug 2026)                   */
/* ws.sinric.pro -> Let's Encrypt YR1 -> ISRG Root YR                     */
/* ----------------------------------------------------------------------- */
static const char k_sinric_ca_pem[] =
    /* ISRG Root YR (root, signed by ISRG Root X1) */
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n"
    /* Let's Encrypt YR1 intermediate */
    "-----BEGIN CERTIFICATE-----\n"
    "MIIE2zCCAsOgAwIBAgIRAKICU/FfJpHAXcHOE7m8yk4wDQYJKoZIhvcNAQELBQAw\n"
    "LjELMAkGA1UEBhMCVVMxDTALBgNVBAoTBElTUkcxEDAOBgNVBAMTB1Jvb3QgWVIw\n"
    "HhcNMjUwOTAzMDAwMDAwWhcNMjgwOTAyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEW\n"
    "MBQGA1UEChMNTGV0J3MgRW5jcnlwdDEMMAoGA1UEAxMDWVIxMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAoVi8X2xCYgMXvJxNPKp/oF13UMgmPABB07VC\n"
    "LNDtoXmt9luEZNJSBV10VyT1Pz6LD8Zq1d2gc43WNl1AdRrj4sEnazbOiz0nPpmG\n"
    "Bp2hui49oZtDIY6wdKeZAi5BbNU20CH6RSBBMLSQ9cXrH8dxdv4PAJ45ssGML68U\n"
    "SE3BsjC2a6cAN9L5CgXVIQi5tfNiTPoFZZ3S0OlXqLmmtdV95udWAb5b6e/F49Di\n"
    "CsH0Y00Ag72BVIb1hzynmKe+X0mERBTtsb3BwmpV9ipeBjMLoR/D9cHxHQCWoi5l\n"
    "TmXwY015J5rGelz1nZjJuxc2kioaX29XJBnhMkP531rSdG5uMwIDAQABo4HuMIHr\n"
    "MA4GA1UdDwEB/wQEAwIBhjATBgNVHSUEDDAKBggrBgEFBQcDATASBgNVHRMBAf8E\n"
    "CDAGAQH/AgEAMB0GA1UdDgQWBBQfLzW+RhSCzUCxrnksVXj699Ro+zAfBgNVHSME\n"
    "GDAWgBTe51tg0CJtQCh9Pw0B/qS1UrRRlDAyBggrBgEFBQcBAQQmMCQwIgYIKwYB\n"
    "BQUHMAKGFmh0dHA6Ly95ci5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwB\n"
    "AgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3lyLmMubGVuY3Iub3JnLzANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEA0+zvMq3kHig1ddTmmm+RibTr9/RpX7k4buanMMRqbV/y\n"
    "IvP82zAHN3mvaw+cASuVsdpd0ikjhr4hnhJQLQOzOp2ccKrsdGOAgo0vddeISFAq\n"
    "EWEkV4lmUM3vFF796up+bSgmJ1u6RupDCMxDgF8M3eLvGuj6L0lu3zkQ0KuQLnKxL\n"
    "tB0oQqn1Idg5CuuGpMvQzk29Pa3D/qHurc0EIM9SxukQuJqq63lxsYyRQFU8yMBO\n"
    "hq1w5LbfaWNRrz1uklOfI/pYkAb2E2MTZrAMQkBIE2S8Jt1F8gRc96o/xOsrgvSk\n"
    "a84AisX6xq1lz1Z7jGvrnXc4TMcjxZTjiTaihcYI1JIXZiLtEMSCa5l3cu8YWd6z\n"
    "dLRQlqRdclVjuQfNHawRJ6GWlkK0QJosivTKwdBw3KxEtzGo8yMHERbsy57gP1UX\n"
    "HOMcmZYQC0gtyR3SxfenIM/MxC3Ia2Ypab/kQ/CTnlIn2KQ5JUC6NYrGCbhFN9bp\n"
    "5lKJStEwCUnLpntcrXk5XVDCNv/5RyWpRThkGOV7GetKkQ0qAY8hCzWK6oqnAhDZ\n"
    "cjlYVdWfqOw3DIOX6EDNBgAqHarRVxyF9QZdOaXSyPJ0ueD2BYJEBgaCGQ8rAaU/\n"
    "Qc123V5LTXDZW4CcsPBDyhy4v+c8hClAyw/IkJlfBqxB9D+/wvIMHgECZ4ptP6o=\n"
    "-----END CERTIFICATE-----\n";

/* ----------------------------------------------------------------------- */
/* Level table                                                              */
/* ----------------------------------------------------------------------- */
typedef struct {
    water_level_t level;
    const char   *label;
    int           percent;
} sinric_level_info_t;

static const sinric_level_info_t k_level_info[] = {
    { WATER_LEVEL_EMPTY,   "Tank Empty",                    0  },
    { WATER_LEVEL_LOW,     "Water Level Low",              22  },
    { WATER_LEVEL_MEDIUM,  "Water Level Sixty One Percent",61  },
    { WATER_LEVEL_FULL,    "Tank Full",                   100  },
    { WATER_LEVEL_INVALID, "Sensor Fault",                  0  },
};

/* ----------------------------------------------------------------------- */
/* Minimal WebSocket framing — RFC 6455, client frames must be masked       */
/* ----------------------------------------------------------------------- */
#define WS_OPCODE_TEXT  0x01
#define WS_FIN          0x80

static int ws_frame_text(uint8_t *out, size_t out_size,
                          const char *payload, size_t payload_len)
{
    if (payload_len > 65535) return -1;

    size_t  header_len;
    uint8_t mask[4];
    uint32_t m = esp_random();
    memcpy(mask, &m, 4);

    out[0] = WS_FIN | WS_OPCODE_TEXT;
    if (payload_len < 126) {
        out[1] = (uint8_t)(0x80 | payload_len);
        memcpy(out + 2, mask, 4);
        header_len = 6;
    } else {
        out[1] = 0x80 | 126;
        out[2] = (payload_len >> 8) & 0xFF;
        out[3] =  payload_len       & 0xFF;
        memcpy(out + 4, mask, 4);
        header_len = 8;
    }

    if (header_len + payload_len > out_size) return -1;

    for (size_t i = 0; i < payload_len; i++) {
        out[header_len + i] = (uint8_t)payload[i] ^ mask[i & 3];
    }
    return (int)(header_len + payload_len);
}

/* ----------------------------------------------------------------------- */
/* Build Sinric Pro JSON payload                                            */
/* ----------------------------------------------------------------------- */
static void build_payload(char *buf, size_t buf_size, int percent)
{
    uint32_t r0 = esp_random(), r1 = esp_random(),
             r2 = esp_random(), r3 = esp_random();

    unsigned long now = (unsigned long)(xTaskGetTickCount() / configTICK_RATE_HZ)
                        + 1704067200UL;

    char msg_id[40], ts[14];
    snprintf(msg_id, sizeof(msg_id),
             "%08lx-%04lx-%04lx-%04lx-%08lx%04lx",
             (unsigned long)r0,
             (unsigned long)((r1 >> 16) & 0xFFFFU),
             (unsigned long)(r1 & 0xFFFFU),
             (unsigned long)((r2 >> 16) & 0xFFFFU),
             (unsigned long)(r2 & 0xFFFFU),
             (unsigned long)(r3 >> 16));
    snprintf(ts, sizeof(ts), "%lu", now);

    snprintf(buf, buf_size,
             "{"
             "\"payloadVersion\":2,"
             "\"clientId\":\"%s\","
             "\"messageId\":\"%s\","
             "\"createdAt\":%s,"
             "\"deviceId\":\"%s\","
             "\"type\":\"event\","
             "\"action\":\"sendTemperatureEvent\","
             "\"value\":{\"temperature\":%d,\"humidity\":%d}"
             "}",
             SINRIC_PRO_APP_KEY,
             msg_id,
             ts,
             SINRIC_PRO_DEVICE_ID,
             percent, percent);
}

/* ----------------------------------------------------------------------- */
/* Tiny inline base64 encoder (16 bytes -> 24-char string + NUL)           */
/* ----------------------------------------------------------------------- */
static void base64_16(const uint8_t *in, char *out)
{
    static const char enc[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, o = 0;
    for (i = 0; i < 15; i += 3) {
        uint32_t v = ((uint32_t)in[i] << 16) |
                     ((uint32_t)in[i+1] << 8) |
                     (uint32_t)in[i+2];
        out[o++] = enc[(v >> 18) & 0x3F];
        out[o++] = enc[(v >> 12) & 0x3F];
        out[o++] = enc[(v >>  6) & 0x3F];
        out[o++] = enc[(v >>  0) & 0x3F];
    }
    uint32_t v = (uint32_t)in[15] << 16;
    out[o++] = enc[(v >> 18) & 0x3F];
    out[o++] = enc[(v >> 12) & 0x3F];
    out[o++] = '=';
    out[o++] = '=';
    out[o]   = '\0';
}

/* ----------------------------------------------------------------------- */
/* Open WSS connection + WebSocket handshake                                */
/* ----------------------------------------------------------------------- */
static esp_tls_t *s_tls = NULL;

static bool sinric_ws_connect(void)
{
    if (s_tls) {
        esp_tls_conn_destroy(s_tls);
        s_tls = NULL;
    }

    esp_tls_cfg_t tls_cfg = {
        .cacert_buf   = (const unsigned char *)k_sinric_ca_pem,
        .cacert_bytes = sizeof(k_sinric_ca_pem),
    };

    s_tls = esp_tls_init();
    if (!s_tls) {
        ESP_LOGE(TAG, "esp_tls_init failed");
        return false;
    }

    int ret = esp_tls_conn_new_sync("ws.sinric.pro", strlen("ws.sinric.pro"),
                                     443, &tls_cfg, s_tls);
    if (ret != 1) {
        ESP_LOGE(TAG, "TLS connect failed: %d", ret);
        esp_tls_conn_destroy(s_tls);
        s_tls = NULL;
        return false;
    }

    /* WebSocket HTTP Upgrade with Sinric Pro auth headers */
    uint8_t key_raw[16];
    uint32_t k0 = esp_random(), k1 = esp_random(),
             k2 = esp_random(), k3 = esp_random();
    memcpy(key_raw,      &k0, 4);
    memcpy(key_raw +  4, &k1, 4);
    memcpy(key_raw +  8, &k2, 4);
    memcpy(key_raw + 12, &k3, 4);
    char key_b64[25];
    base64_16(key_raw, key_b64);

    char http_req[768];
    int  req_len = snprintf(http_req, sizeof(http_req),
        "GET / HTTP/1.1\r\n"
        "Host: ws.sinric.pro\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "appkey: %s\r\n"
        "deviceids: %s\r\n"
        "platform: ESP32\r\n"
        "version: 2.10.0\r\n"
        "restoredevicestates: false\r\n"
        "\r\n",
        key_b64,
        SINRIC_PRO_APP_KEY,
        SINRIC_PRO_DEVICE_ID);

    ssize_t written = esp_tls_conn_write(s_tls, http_req, req_len);
    if (written < req_len) {
        ESP_LOGE(TAG, "WS handshake write failed (%d of %d)", (int)written, req_len);
        esp_tls_conn_destroy(s_tls);
        s_tls = NULL;
        return false;
    }

    char resp[512] = {0};
    ssize_t read_len = esp_tls_conn_read(s_tls, resp, sizeof(resp) - 1);
    if (read_len <= 0 || strstr(resp, "101") == NULL) {
        ESP_LOGE(TAG, "WS upgrade failed (rx=%d): %.80s", (int)read_len, resp);
        esp_tls_conn_destroy(s_tls);
        s_tls = NULL;
        return false;
    }

    ESP_LOGI(TAG, "WebSocket connected to ws.sinric.pro");
    return true;
}

static bool sinric_ws_send(const char *payload)
{
    if (!s_tls) return false;
    size_t  plen = strlen(payload);
    size_t  frame_size = plen + 10;
    uint8_t *frame = malloc(frame_size);
    if (!frame) return false;

    int flen = ws_frame_text(frame, frame_size, payload, plen);
    bool ok = false;
    if (flen > 0) {
        ssize_t w = esp_tls_conn_write(s_tls, frame, flen);
        ok = (w == flen);
    }
    free(frame);
    return ok;
}

/* ----------------------------------------------------------------------- */
/* sinric_task                                                              */
/* ----------------------------------------------------------------------- */
static void sinric_task(void *arg)
{
    (void)arg;

    xEventGroupWaitBits(g_system_event_group,
                        EVT_WIFI_CONNECTED,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Wi-Fi connected — starting Sinric Pro WebSocket...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    char payload[600];

    for (;;) {
        if (!sinric_ws_connect()) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        int percent = k_level_info[(int)g_current_level].percent;
        build_payload(payload, sizeof(payload), percent);
        if (sinric_ws_send(payload)) {
            ESP_LOGI(TAG, "Sinric Pro: initial state sent (%d%%)", percent);
        }

        level_event_t evt;
        TickType_t    last_hb   = xTaskGetTickCount();
        bool          connected = true;

        while (connected) {
            bool got_event = (xQueueReceive(g_level_change_queue, &evt,
                                            pdMS_TO_TICKS(5000)) == pdTRUE);
            TickType_t now = xTaskGetTickCount();

            if (got_event) {
                int p = k_level_info[(int)evt.level].percent;
                build_payload(payload, sizeof(payload), p);
                if (!sinric_ws_send(payload)) {
                    ESP_LOGW(TAG, "Send failed — reconnecting");
                    connected = false;
                } else {
                    ESP_LOGI(TAG, "Sinric Pro: level event sent (%d%%)", p);
                    last_hb = now;
                }
            } else if ((now - last_hb) >= pdMS_TO_TICKS(20000)) {
                int p = k_level_info[(int)g_current_level].percent;
                build_payload(payload, sizeof(payload), p);
                if (!sinric_ws_send(payload)) {
                    ESP_LOGW(TAG, "Heartbeat failed — reconnecting");
                    connected = false;
                } else {
                    ESP_LOGI(TAG, "Sinric Pro: heartbeat (%d%%)", p);
                    last_hb = now;
                }
            }
        }

        if (s_tls) {
            esp_tls_conn_destroy(s_tls);
            s_tls = NULL;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t sinric_client_init(void)
{
    BaseType_t res = xTaskCreate(sinric_task, "sinric_task", 6144, NULL, 2, NULL);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}
