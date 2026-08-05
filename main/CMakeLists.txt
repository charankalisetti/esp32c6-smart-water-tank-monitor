# main/CMakeLists.txt
# ESP-IDF main component registration for the Water Tank Level Monitor.

idf_component_register(
    SRCS
        "main.c"
        "app_main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        freertos
        log
        esp_common
        services
        drivers
        cloud
)
