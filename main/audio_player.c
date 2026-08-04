/**
 * @file audio_player.c
 * @brief FreeRTOS task that owns the ESP-IDF I2S Standard Driver and plays
 *        embedded PCM announcement clips in response to water level events.
 *
 * Architecture:
 *   audio_player_task()
 *     └─ Blocks on g_level_change_queue (portMAX_DELAY)
 *     └─ On receipt of level_event_t:
 *          - Looks up the PCM clip for the level
 *          - Sets EVT_AUDIO_PLAYING
 *          - Writes PCM to I2S in AUDIO_CHUNK_BYTES chunks
 *          - Clears EVT_AUDIO_PLAYING
 *          - Logs "Audio finished"
 *
 * I2S Standard Mode (ESP-IDF i2s_std driver):
 *   - NOT the legacy i2s.h driver
 *   - I2S_SLOT_MODE_MONO with Philips format
 *   - MAX98357A operates in mono mode; the LRC signal selects left channel
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include "audio_player.h"
#include "app_events.h"
#include "gpio_config.h"

#include "audio/tank_empty.h"
#include "audio/water_low.h"
#include "audio/water_medium.h"
#include "audio/tank_full.h"

#include "audio/tank_empty_te.h"
#include "audio/water_low_te.h"
#include "audio/water_medium_te.h"
#include "audio/tank_full_te.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

static const char *TAG = "AUDIO_PLAYER";

/* =========================================================================
 * Module-private state
 * ========================================================================= */

/** I2S TX channel handle — owned exclusively by this module. */
static i2s_chan_handle_t s_i2s_tx_handle = NULL;

/** True after audio_player_init() succeeds. */
static bool s_initialized = false;

/* =========================================================================
 * PCM clip descriptor table
 * Maps water_level_t → English + Telugu PCM arrays and metadata
 * ========================================================================= */

typedef struct {
    water_level_t   level;
    const int16_t  *pcm_en;
    const char     *label_en;
    const int16_t  *pcm_te;
    const char     *label_te;
} clip_entry_t;

static const clip_entry_t CLIP_TABLE[] = {
    { WATER_LEVEL_EMPTY,   tank_empty_pcm,   "Tank Empty",                    tank_empty_te_pcm,   "ట్యాంక్ ఖాళీగా ఉంది"                    },
    { WATER_LEVEL_LOW,     water_low_pcm,    "Water Level Low",               water_low_te_pcm,    "నీటి మట్టం తక్కువగా ఉంది"               },
    { WATER_LEVEL_MEDIUM,  water_medium_pcm, "Water Level Sixty One Percent", water_medium_te_pcm, "నీటి మట్టం అరవై ఒక్క శాతం ఉంది" },
    { WATER_LEVEL_FULL,    tank_full_pcm,    "Tank Full",                     tank_full_te_pcm,    "ట్యాంక్ నిండిపోయింది"                     },
};

/* =========================================================================
 * I2S Driver Initialization
 * ========================================================================= */

/**
 * @brief Configure and enable the I2S TX channel for the MAX98357A.
 *
 * Uses ESP-IDF Standard I2S driver (driver/i2s_std.h), not the legacy driver.
 *
 * @return ESP_OK on success.
 */
static esp_err_t i2s_driver_init(void)
{
    /* 1. Allocate I2S TX-only channel on auto-selected controller */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, &s_i2s_tx_handle, NULL),
        TAG, "i2s_new_channel() failed"
    );

    /* 2. Configure clock, slot format, and GPIO mapping */
    i2s_std_config_t std_cfg = {
        /* Clock: 16 kHz sample rate */
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),

        /* Slot: Standard Philips, 16-bit, Mono. */
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT,
                        I2S_SLOT_MODE_MONO),

        /* GPIO mapping — matches FINAL GPIO ASSIGNMENTS in the spec */
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,        /* MAX98357A does not need MCLK */
            .bclk = I2S_GPIO_BCLK,          /* GPIO19                       */
            .ws   = I2S_GPIO_LRC,           /* GPIO18                       */
            .dout = I2S_GPIO_DOUT,          /* GPIO20                       */
            .din  = I2S_GPIO_UNUSED,        /* TX only — no input           */
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_i2s_tx_handle, &std_cfg),
        TAG, "i2s_channel_init_std_mode() failed"
    );

    /* 3. Enable the channel — DMA starts, I2S clock runs */
    ESP_RETURN_ON_ERROR(
        i2s_channel_enable(s_i2s_tx_handle),
        TAG, "i2s_channel_enable() failed"
    );

    ESP_LOGI(TAG, "I2S initialized: %u Hz, 16-bit Mono, "
                  "BCLK=GPIO%d, LRC=GPIO%d, DIN=GPIO%d",
             AUDIO_SAMPLE_RATE_HZ,
             I2S_GPIO_BCLK, I2S_GPIO_LRC, I2S_GPIO_DOUT);

    return ESP_OK;
}

/* =========================================================================
 * Audio Playback Helpers
 * ========================================================================= */

/**
 * @brief Stream a PCM sample array to I2S with +6 dB (2.0x) software digital gain boost.
 */
static esp_err_t play_pcm_raw(const int16_t *pcm, size_t sample_count, const char *label)
{
    if (pcm == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t pcm_bytes = sample_count * sizeof(int16_t);
    ESP_LOGI(TAG, "Playing (MAX LOUDNESS): \"%s\" (%zu samples, %zu bytes)", label, sample_count, pcm_bytes);

    const TickType_t write_timeout = pdMS_TO_TICKS(AUDIO_I2S_WRITE_TIMEOUT_MS);
    size_t sample_offset = 0;
    int16_t chunk_buf[256]; /* 512 bytes chunk buffer */

    while (sample_offset < sample_count) {
        size_t samples_to_process = sample_count - sample_offset;
        if (samples_to_process > 256) {
            samples_to_process = 256;
        }

        /* Apply +12 dB (4x) gain multiplier with saturation clamping */
        for (size_t i = 0; i < samples_to_process; i++) {
            int32_t val = (int32_t)pcm[sample_offset + i] * 4;
            if (val > 32767)  val = 32767;
            if (val < -32768) val = -32768;
            chunk_buf[i] = (int16_t)val;
        }

        size_t bytes_to_write = samples_to_process * sizeof(int16_t);
        size_t bytes_written = 0;

        esp_err_t ret = i2s_channel_write(
            s_i2s_tx_handle,
            (const uint8_t *)chunk_buf,
            bytes_to_write,
            &bytes_written,
            write_timeout
        );

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2s_channel_write() error: %s", esp_err_to_name(ret));
            return ret;
        }

        sample_offset += (bytes_written / sizeof(int16_t));
    }

    ESP_LOGI(TAG, "Audio finished: \"%s\"", label);
    return ESP_OK;
}

/**
 * @brief Look up clip descriptor.
 */
static const clip_entry_t *find_clip(water_level_t level)
{
    for (size_t i = 0; i < sizeof(CLIP_TABLE) / sizeof(CLIP_TABLE[0]); i++) {
        if (CLIP_TABLE[i].level == level) {
            return &CLIP_TABLE[i];
        }
    }
    return NULL;
}

/* =========================================================================
 * FreeRTOS Task Implementation
 * ========================================================================= */

/**
 * @brief Audio player FreeRTOS task.
 *
 * Plays English announcement first, followed by Telugu announcement.
 */
static void audio_player_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Task started — waiting for level change events");

    while (1) {
        level_event_t event;

        /* Block indefinitely until a level change event arrives */
        if (xQueueReceive(g_level_change_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Skip invalid sensor-fault events */
        if (event.level == WATER_LEVEL_INVALID) {
            ESP_LOGW(TAG, "Received INVALID level event — skipping audio");
            continue;
        }

        /* Find the matching bilingual clip entry */
        const clip_entry_t *clip = find_clip(event.level);
        if (clip == NULL) {
            ESP_LOGE(TAG, "No audio clip found for level %d", (int)event.level);
            continue;
        }

        /* Determine lengths based on level */
        size_t len_en = 0, len_te = 0;
        if (event.level == WATER_LEVEL_EMPTY) {
            len_en = tank_empty_pcm_len;
            len_te = tank_empty_te_pcm_len;
        } else if (event.level == WATER_LEVEL_LOW) {
            len_en = water_low_pcm_len;
            len_te = water_low_te_pcm_len;
        } else if (event.level == WATER_LEVEL_MEDIUM) {
            len_en = water_medium_pcm_len;
            len_te = water_medium_te_pcm_len;
        } else if (event.level == WATER_LEVEL_FULL) {
            len_en = tank_full_pcm_len;
            len_te = tank_full_te_pcm_len;
        }

        ESP_LOGI(TAG, "Bilingual announcement triggered for level %d", (int)event.level);

        /* Mark audio as in-progress */
        xEventGroupSetBits(g_system_event_group, EVT_AUDIO_PLAYING);

        /* 1. Play English announcement */
        play_pcm_raw(clip->pcm_en, len_en, clip->label_en);

        /* Short 300 ms inter-phrase pause */
        vTaskDelay(pdMS_TO_TICKS(300));

        /* 2. Play Telugu announcement */
        play_pcm_raw(clip->pcm_te, len_te, clip->label_te);

        /* Mark audio as idle */
        /* Mark audio as idle */
        xEventGroupClearBits(g_system_event_group, EVT_AUDIO_PLAYING);
    }

    vTaskDelete(NULL);
}

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

esp_err_t audio_player_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "audio_player_init() called more than once — ignoring");
        return ESP_OK;
    }

    /* Initialize the I2S driver and hardware */
    esp_err_t ret = i2s_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S driver initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Signal to the rest of the system that the audio path is ready */
    xEventGroupSetBits(g_system_event_group, EVT_I2S_READY);
    ESP_LOGI(TAG, "EVT_I2S_READY set");

    /* Create the audio player task */
    BaseType_t result = xTaskCreate(
        audio_player_task,
        "audio_player",
        AUDIO_PLAYER_TASK_STACK,
        NULL,
        AUDIO_PLAYER_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed for audio_player_task "
                      "(stack=%u, priority=%u)",
                 AUDIO_PLAYER_TASK_STACK, AUDIO_PLAYER_TASK_PRIORITY);
        /* Driver is up but task failed — deinit to avoid resource leak */
        i2s_channel_disable(s_i2s_tx_handle);
        i2s_del_channel(s_i2s_tx_handle);
        s_i2s_tx_handle = NULL;
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "audio_player_task created (stack=%u bytes, priority=%u)",
             AUDIO_PLAYER_TASK_STACK, AUDIO_PLAYER_TASK_PRIORITY);

    return ESP_OK;
}

esp_err_t audio_player_deinit(void)
{
    if (!s_initialized || s_i2s_tx_handle == NULL) {
        return ESP_OK;
    }

    i2s_channel_disable(s_i2s_tx_handle);
    i2s_del_channel(s_i2s_tx_handle);
    s_i2s_tx_handle = NULL;
    s_initialized   = false;

    ESP_LOGI(TAG, "I2S driver released");
    return ESP_OK;
}
