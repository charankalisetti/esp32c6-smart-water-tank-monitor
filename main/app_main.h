/**
 * @file app_main.h
 * @brief System orchestration interface.
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all subsystems and start FreeRTOS tasks.
 *
 * Called from the ESP-IDF app_main() entry point in main.c.
 * Returns after all tasks are created; the scheduler takes over.
 *
 * @return ESP_OK on success, or an error code if any subsystem fails to init.
 */
esp_err_t app_main_run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
