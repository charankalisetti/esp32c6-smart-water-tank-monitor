/**
 * @file sinric_client.h
 * @brief Sinric Pro Google Home & Google Assistant integration task.
 */

#ifndef SINRIC_CLIENT_H
#define SINRIC_CLIENT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Sinric Pro task to sync live telemetry with Google Home.
 */
esp_err_t sinric_client_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SINRIC_CLIENT_H */
