#ifndef NIGHT_SLEEP_H
#define NIGHT_SLEEP_H

#include "esp_err.h"

/**
 * Initialize timezone (IST UTC+5:30) and log wakeup cause.
 */
void night_sleep_init(void);

/**
 * Checks if current time is within night rest hours (23:00 to 04:00 IST).
 * If yes, calculates seconds until 04:00 AM IST, configures wakeups,
 * and enters Deep Sleep.
 */
void night_sleep_check_and_enter(void);

/**
 * Starts a background task that checks every minute if it's 11:00 PM to enter Deep Sleep.
 */
void night_sleep_start_monitor_task(void);

#endif // NIGHT_SLEEP_H
