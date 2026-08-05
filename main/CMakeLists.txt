# main/CMakeLists.txt
# ESP-IDF component registration for the Water Tank Level Monitor.
#
# Source files are listed explicitly (no glob) for reproducible builds.
# audio/audio_clips.c is compiled as a regular C source — the PCM arrays
# are stored in .rodata (flash) via the const qualifier, not as EMBED_FILES,
# because they are accessed as typed int16_t arrays (not raw byte blobs).

idf_component_register(
    SRCS
        "main.c"
        "app_main.c"
        "app_events.c"
        "gpio_config.c"
        "water_sensor.c"
        "audio_player.c"
        "audio/audio_clips.c"
        "audio/tank_empty_te.c"
        "audio/water_low_te.c"
        "audio/water_medium_te.c"
        "audio/tank_full_te.c"
        "wifi_manager.c"
        "wifi_prov.c"
        "blynk_client.c"
        "sinric_client.c"
        "night_sleep.c"

    INCLUDE_DIRS
        "."
        "audio"

    REQUIRES
        driver
        esp_ringbuf
        freertos
        log
        esp_common
        # Wi-Fi + networking + BLE Provisioning
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        protocomm
        bt
        # HTTPS client + TLS (esp_tls is a transitive dep of esp_http_client)
        esp_http_client
        mbedtls
        lwip
)
