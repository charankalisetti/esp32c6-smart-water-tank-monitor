/**
 * @file audio_player.h
 * @brief I2S audio playback task interface for the Water Tank Level Monitor.
 *
 * Owns the ESP-IDF I2S Standard Driver (i2s_std) and plays embedded PCM
 * audio clips in response to water level change events received from
 * g_level_change_queue.
 *
 * I2S Configuration (MAX98357A):
 *   BCLK  → GPIO19
 *   LRC   → GPIO18
 *   DIN   → GPIO20
 *   Mode  → Standard Philips, 16-bit, Mono
 *   Rate  → 16000 Hz (matches embedded PCM clips)
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * I2S Audio Configuration Constants
 * ========================================================================= */

/** Sample rate of the embedded PCM clips and I2S peripheral clock. */
#define AUDIO_SAMPLE_RATE_HZ    16000u

/**
 * @brief I2S write chunk size in bytes.
 *
 * 512 bytes = 256 int16_t samples = 16 ms of audio at 16 kHz.
 * Small enough to avoid starving FreeRTOS scheduler, large enough to
 * keep I2S DMA buffers full without constant context switches.
 */
#define AUDIO_CHUNK_BYTES       512u

/**
 * @brief Number of repetitions to loop the short placeholder PCM clip.
 *
 * 64 samples × 250 loops = 16000 samples = 1.0 s at 16 kHz.
 * When real TTS clips are embedded, set this to 1 (no looping needed).
 */
#define AUDIO_CLIP_LOOP_COUNT   1u

/** Timeout in ms for i2s_channel_write() — prevents infinite block. */
#define AUDIO_I2S_WRITE_TIMEOUT_MS  200u

/* =========================================================================
 * Task Configuration
 * ========================================================================= */

/** Stack size for the audio player task (bytes).
 *  I2S driver + DMA buffers need more stack than the sensor task. */
#define AUDIO_PLAYER_TASK_STACK     4096u

/** Priority — slightly higher than sensor task so audio starts promptly. */
#define AUDIO_PLAYER_TASK_PRIORITY  3u

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Initialize the I2S standard driver and create the audio player task.
 *
 * Configures I2S with:
 *  - Standard Philips slot format
 *  - 16-bit mono PCM
 *  - 16000 Hz sample rate
 *  - GPIO18 (LRC), GPIO19 (BCLK), GPIO20 (DIN)
 *
 * Sets EVT_I2S_READY in g_system_event_group on success.
 *
 * Must be called after app_events_init().
 *
 * @return ESP_OK on success, or esp_err_t on I2S/task creation failure.
 */
esp_err_t audio_player_init(void);

/**
 * @brief Gracefully stop the I2S driver and release resources.
 *
 * Intended for clean shutdown; not normally called in embedded applications.
 *
 * @return ESP_OK on success.
 */
esp_err_t audio_player_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PLAYER_H */
