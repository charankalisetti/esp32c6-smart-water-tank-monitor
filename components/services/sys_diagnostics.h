/**
 * @file sys_diagnostics.h
 * @brief Real-time system health diagnostics & telemetry monitor.
 *
 * @author Principal Embedded Systems Engineer
 */

#ifndef SYS_DIAGNOSTICS_H
#define SYS_DIAGNOSTICS_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t free_heap;
    uint32_t min_free_heap;
    int      wifi_rssi;
    uint32_t uptime_sec;
} sys_health_metrics_t;

/**
 * @brief Initialize and spawn the background 60-second system diagnostics monitor task.
 */
esp_err_t sys_diagnostics_init(void);

/**
 * @brief Get current snapshot of system health metrics.
 */
void sys_diagnostics_get_metrics(sys_health_metrics_t *out_metrics);

#ifdef __cplusplus
}
#endif

#endif /* SYS_DIAGNOSTICS_H */
