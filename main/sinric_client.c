/**
 * @file sinric_client.c
 * @brief Sinric Pro Google Home integration via WebSocket over esp_tls.
 *
 * Implements RFC-6455 WebSocket + correct Sinric Pro message envelope:
 *   { "payloadVersion":2, "signatureVersion":12,
 *     "signature":{"HMAC":"<sha256_hex>"},
 *     "payload":{ ... "action":"sendTemperatureEvent" ... } }
 *
 * HMAC-SHA256 is computed with APP_SECRET as key, payload JSON as message.
 * Without this signature the Sinric Pro server discards the events (shows "--").
 *
 * TLS: ISRG Root YR + Let's Encrypt YR1  (ws.sinric.pro chain, Aug 2026)
 */

#include "sinric_client.h"
#include "wifi_config.h"
#include "app_events.h"
#include "water_sensor.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_random.h"
#include "mbedtls/md.h"
#include "psa/crypto.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"

static const char *TAG = "SINRIC_PRO";

/* ----------------------------------------------------------------------- */
/* TLS trust chain for ws.sinric.pro (verified Aug 2026)                   */
/* ws.sinric.pro -> Let's Encrypt YR1 -> ISRG Root YR                     */
/* ----------------------------------------------------------------------- */
static const char k_sinric_ca_pem[] =
    /* ISRG Root YR */
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
typedef struct { water_level_t level; int percent; } sinric_level_t;
static const sinric_level_t k_lvl[] = {
    { WATER_LEVEL_EMPTY,   0   },
    { WATER_LEVEL_LOW,    22   },
    { WATER_LEVEL_MEDIUM, 61   },
    { WATER_LEVEL_FULL,  100   },
    { WATER_LEVEL_INVALID, 0   },
};

/* ----------------------------------------------------------------------- */
/* HMAC-SHA256 using mbedtls — returns BASE64 encoded string               */
/* The official Sinric Pro SDK uses HMACbase64(), NOT hex encoding.         */
/* 32 HMAC bytes -> 44-char base64 string + '\0'                           */
/* ----------------------------------------------------------------------- */
static void hmac_sha256_base64(const char *key, const char *msg, char *out_b64)
{
    psa_crypto_init();

    uint8_t mac_out[32] = {0};
    
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);

    psa_key_id_t key_id;
    psa_status_t status = psa_import_key(&attributes, (const uint8_t*)key, strlen(key), &key_id);
    
    if (status == PSA_SUCCESS) {
        size_t mac_length = 0;
        status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256), 
                                 (const uint8_t*)msg, strlen(msg), 
                                 mac_out, sizeof(mac_out), &mac_length);
        if (status != PSA_SUCCESS) {
            ESP_LOGE(TAG, "psa_mac_compute failed: %d", status);
        }
        psa_destroy_key(key_id);
    } else {
        ESP_LOGE(TAG, "psa_import_key failed: %d", status);
    }

    char hex_str[65];
    for(int i=0; i<32; i++) sprintf(hex_str + i*2, "%02x", mac_out[i]);
    ESP_LOGI(TAG, "RAW MAC HEX: %s", hex_str);

    size_t olen = 0;
    mbedtls_base64_encode((uint8_t *)out_b64, 48, &olen, mac_out, 32);
    out_b64[olen] = '\0';
}

/* ----------------------------------------------------------------------- */
/* Build the full signed Sinric Pro envelope                               */
/*                                                                         */
/* Wire format (matching official SDK):                                    */
/*   {                                                                     */
/*     "payloadVersion":2,                                                 */
/*     "signatureVersion":12,                                              */
/*     "signature":{"HMAC":"<64-char hex>"},                               */
/*     "payload":{                                                         */
/*       "action":"sendTemperatureEvent",                                  */
/*       "clientId":"<APP_KEY>",                                           */
/*       "createdAt":<unix_ts>,                                            */
/*       "deviceAttributes":[],                                            */
/*       "deviceId":"<DEVICE_ID>",                                         */
/*       "reachability":true,                                              */
/*       "type":"event",                                                   */
/*       "value":{"humidity":<n>,"temperature":<n>}                        */
/*     }                                                                   */
/*   }                                                                     */
/* ----------------------------------------------------------------------- */
static void build_signed_message(char *out, size_t out_size, int percent)
{
    /* 1. Build the payload JSON string first */
    /* Must exactly match the insertion order of ArduinoJson used in the official SDK */
    char reply_token[37];
    snprintf(reply_token, sizeof(reply_token), "%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
             (unsigned int)(esp_random() & 0xFFFF), (unsigned int)(esp_random() & 0xFFFF), (unsigned int)(esp_random() & 0xFFFF),
             (unsigned int)((esp_random() & 0x0FFF) | 0x4000),
             (unsigned int)((esp_random() & 0x3FFF) | 0x8000),
             (unsigned int)(esp_random() & 0xFFFF), (unsigned int)(esp_random() & 0xFFFF), (unsigned int)(esp_random() & 0xFFFF));

    time_t now = time(NULL);
    uint64_t created_at = (uint64_t)now;
    if (created_at < 1700000000ULL) {
        created_at = 1785838000ULL; /* Fallback to current August 2026 epoch timestamp */
    }

    char payload[384];
    snprintf(payload, sizeof(payload),
             "{\"action\":\"currentTemperature\",\"cause\":{\"type\":\"PERIODIC_POLL\"},\"createdAt\":%llu,\"deviceId\":\"%s\",\"replyToken\":\"%s\",\"type\":\"event\",\"value\":{\"humidity\":%d,\"temperature\":%d}}",
             (unsigned long long)created_at,
             SINRIC_PRO_DEVICE_ID,
             reply_token,
             (int)percent,
             (int)percent);

    /* 2. Sign payload with HMAC-SHA256(APP_SECRET, payload) — base64 encoded.
     *    The Sinric Pro SDK uses HMACbase64(), so the output must be base64
     *    (44 chars), not hex (64 chars). Server rejects non-base64 signatures. */
    char hmac[48];
    hmac_sha256_base64(SINRIC_PRO_APP_SECRET, payload, hmac);

    /* 3. Wrap in full Sinric Pro envelope */
    snprintf(out, out_size,
             "{\"header\":{\"payloadVersion\":2,\"signatureVersion\":1},\"payload\":%s,\"signature\":{\"HMAC\":\"%s\"}}",
             payload, hmac);

    ESP_LOGI(TAG, "Sinric Payload: %s", payload);
    ESP_LOGI(TAG, "Sinric HMAC: %s", hmac);
    ESP_LOGI(TAG, "Sinric Envelope: %s", out);
}

/* ----------------------------------------------------------------------- */
/* Minimal WebSocket TX framing — RFC 6455, client frames must be masked    */
/* ----------------------------------------------------------------------- */
#define WS_OP_TEXT  0x01
#define WS_OP_PONG  0x0A
#define WS_OP_PING  0x09
#define WS_OP_CLOSE 0x08
#define WS_FIN      0x80

static int ws_build_frame(uint8_t opcode, const uint8_t *payload,
                           size_t plen, uint8_t *out, size_t out_size)
{
    size_t header_len;
    uint8_t mask[4];
    uint32_t m = esp_random();
    memcpy(mask, &m, 4);

    out[0] = WS_FIN | (opcode & 0x0F);
    if (plen < 126) {
        out[1] = (uint8_t)(0x80 | plen);
        memcpy(out + 2, mask, 4);
        header_len = 6;
    } else if (plen <= 65535) {
        out[1] = 0x80 | 126;
        out[2] = (plen >> 8) & 0xFF;
        out[3] =  plen       & 0xFF;
        memcpy(out + 4, mask, 4);
        header_len = 8;
    } else {
        return -1;
    }
    if (header_len + plen > out_size) return -1;
    for (size_t i = 0; i < plen; i++) {
        out[header_len + i] = payload[i] ^ mask[i & 3];
    }
    return (int)(header_len + plen);
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
        uint32_t v = ((uint32_t)in[i]<<16) | ((uint32_t)in[i+1]<<8) | in[i+2];
        out[o++] = enc[(v>>18)&0x3F]; out[o++] = enc[(v>>12)&0x3F];
        out[o++] = enc[(v>> 6)&0x3F]; out[o++] = enc[(v>> 0)&0x3F];
    }
    uint32_t v = (uint32_t)in[15] << 16;
    out[o++] = enc[(v>>18)&0x3F]; out[o++] = enc[(v>>12)&0x3F];
    out[o++] = '='; out[o++] = '='; out[o] = '\0';
}

/* ----------------------------------------------------------------------- */
/* Connection state                                                         */
/* ----------------------------------------------------------------------- */
static esp_tls_t *s_tls    = NULL;
static int        s_sockfd = -1;

/* ----------------------------------------------------------------------- */
/* WebSocket connect + HTTP upgrade                                         */
/* ----------------------------------------------------------------------- */
static bool sinric_ws_connect(void)
{
    if (s_tls) { esp_tls_conn_destroy(s_tls); s_tls = NULL; }
    s_sockfd = -1;

    esp_tls_cfg_t tls_cfg = {
        .cacert_buf   = (const unsigned char *)k_sinric_ca_pem,
        .cacert_bytes = sizeof(k_sinric_ca_pem),
    };

    s_tls = esp_tls_init();
    if (!s_tls) { ESP_LOGE(TAG, "esp_tls_init failed"); return false; }

    int ret = esp_tls_conn_new_sync("ws.sinric.pro", strlen("ws.sinric.pro"),
                                     443, &tls_cfg, s_tls);
    if (ret != 1) {
        ESP_LOGE(TAG, "TLS connect failed: %d", ret);
        esp_tls_conn_destroy(s_tls); s_tls = NULL; return false;
    }
    esp_tls_get_conn_sockfd(s_tls, &s_sockfd);

    /* Build WebSocket key */
    uint8_t key_raw[16];
    uint32_t k0=esp_random(), k1=esp_random(), k2=esp_random(), k3=esp_random();
    memcpy(key_raw,    &k0,4); memcpy(key_raw+4, &k1,4);
    memcpy(key_raw+8,  &k2,4); memcpy(key_raw+12,&k3,4);
    char key_b64[25]; base64_16(key_raw, key_b64);

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
        key_b64, SINRIC_PRO_APP_KEY, SINRIC_PRO_DEVICE_ID);

    if (esp_tls_conn_write(s_tls, http_req, req_len) < req_len) {
        ESP_LOGE(TAG, "WS handshake write failed");
        esp_tls_conn_destroy(s_tls); s_tls = NULL; return false;
    }

    char resp[512] = {0};
    ssize_t rlen = esp_tls_conn_read(s_tls, resp, sizeof(resp)-1);
    if (rlen <= 0 || strstr(resp, "101") == NULL) {
        ESP_LOGE(TAG, "WS upgrade failed (rx=%d): %.80s", (int)rlen, resp);
        esp_tls_conn_destroy(s_tls); s_tls = NULL; return false;
    }
    ESP_LOGI(TAG, "WebSocket connected to ws.sinric.pro");
    return true;
}

/* ----------------------------------------------------------------------- */
/* Send a masked text frame                                                 */
/* ----------------------------------------------------------------------- */
static bool sinric_ws_send(const char *text)
{
    if (!s_tls) return false;
    size_t  plen = strlen(text);
    size_t  frame_size = plen + 12;
    uint8_t *frame = malloc(frame_size);
    if (!frame) return false;

    int flen = ws_build_frame(WS_OP_TEXT, (const uint8_t *)text, plen,
                               frame, frame_size);
    bool ok = (flen > 0) && (esp_tls_conn_write(s_tls, frame, flen) == flen);
    free(frame);
    return ok;
}

/* ----------------------------------------------------------------------- */
/* Poll for incoming server frames; reply to pings with pong.              */
/* ----------------------------------------------------------------------- */
static bool sinric_ws_poll(void)
{
    if (!s_tls || s_sockfd < 0) return false;

    fd_set rfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&rfds); FD_SET(s_sockfd, &rfds);
    if (select(s_sockfd + 1, &rfds, NULL, NULL, &tv) <= 0) return true;

    uint8_t hdr[2];
    if (esp_tls_conn_read(s_tls, hdr, 2) != 2) return false;

    uint8_t opcode  = hdr[0] & 0x0F;
    bool    masked  = (hdr[1] & 0x80) != 0;
    size_t  plen    = hdr[1] & 0x7F;

    if (plen == 126) {
        uint8_t ext[2];
        if (esp_tls_conn_read(s_tls, ext, 2) != 2) return false;
        plen = ((size_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        uint8_t ext[8]; esp_tls_conn_read(s_tls, ext, 8);
        return true;
    }

    uint8_t mask[4] = {0};
    if (masked) esp_tls_conn_read(s_tls, mask, 4);

    uint8_t payload[256] = {0};
    size_t  to_read = plen < sizeof(payload) ? plen : sizeof(payload);
    if (to_read > 0) {
        esp_tls_conn_read(s_tls, payload, to_read);
        if (masked) for (size_t i=0;i<to_read;i++) payload[i]^=mask[i&3];
    }
    size_t remaining = plen - to_read;
    while (remaining > 0) {
        uint8_t sink[64];
        size_t chunk = remaining < sizeof(sink) ? remaining : sizeof(sink);
        ssize_t rd = esp_tls_conn_read(s_tls, sink, chunk);
        if (rd <= 0) break;
        remaining -= (size_t)rd;
    }

    if (opcode == WS_OP_PING) {
        uint8_t pong_frame[64];
        int flen = ws_build_frame(WS_OP_PONG, payload, to_read,
                                  pong_frame, sizeof(pong_frame));
        if (flen > 0) esp_tls_conn_write(s_tls, pong_frame, flen);
        ESP_LOGD(TAG, "pong sent");
    } else if (opcode == WS_OP_CLOSE) {
        ESP_LOGW(TAG, "WS close — reconnecting");
        return false;
    }
    return true;
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

    char message[900];  /* envelope: ~64-char HMAC + ~512-char payload */

    for (;;) {
        if (!sinric_ws_connect()) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        /* Send initial signed state */
        int percent = k_lvl[(int)g_current_level].percent;
        build_signed_message(message, sizeof(message), percent);
        if (sinric_ws_send(message)) {
            ESP_LOGI(TAG, "Sinric Pro: initial state sent (%d%%)", percent);
        }

        level_event_t evt;
        TickType_t    last_hb   = xTaskGetTickCount();
        bool          connected = true;

        while (connected) {
            /* Poll 1-second window in 50ms slices, servicing pings */
            for (int slice = 0; slice < 20; slice++) {
                bool got = (xQueueReceive(g_level_change_queue, &evt,
                                          pdMS_TO_TICKS(50)) == pdTRUE);
                if (got) {
                    int p = k_lvl[(int)evt.level].percent;
                    build_signed_message(message, sizeof(message), p);
                    if (!sinric_ws_send(message)) {
                        ESP_LOGW(TAG, "Send failed — reconnecting");
                        connected = false;
                    } else {
                        ESP_LOGI(TAG, "Sinric Pro: level event (%d%%)", p);
                        last_hb = xTaskGetTickCount();
                    }
                    break;
                }
                if (!sinric_ws_poll()) { connected = false; break; }
            }

            /* 20-second heartbeat */
            TickType_t now = xTaskGetTickCount();
            if (connected && (now - last_hb) >= pdMS_TO_TICKS(20000)) {
                int p = k_lvl[(int)g_current_level].percent;
                build_signed_message(message, sizeof(message), p);
                if (!sinric_ws_send(message)) {
                    ESP_LOGW(TAG, "Heartbeat failed — reconnecting");
                    connected = false;
                } else {
                    ESP_LOGI(TAG, "Sinric Pro: heartbeat (%d%%)", p);
                    last_hb = now;
                }
            }
        }

        if (s_tls) { esp_tls_conn_destroy(s_tls); s_tls = NULL; }
        s_sockfd = -1;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t sinric_client_init(void)
{
    BaseType_t res = xTaskCreate(sinric_task, "sinric_task", 6144, NULL, 2, NULL);
    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}
