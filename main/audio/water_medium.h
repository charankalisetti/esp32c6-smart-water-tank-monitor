/**
 * @file water_medium.h
 * @brief Embedded PCM audio — "Water Level Sixty One Percent" announcement.
 *
 * Format : 16-bit signed PCM, Mono, 16000 Hz sample rate
 * Content: Synthesized placeholder waveform.
 *
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │  REPLACEMENT INSTRUCTIONS                                              │
 * │                                                                        │
 * │  Record or synthesize "Water Level Sixty One Percent" at 16000 Hz.   │
 * │                                                                        │
 * │  ffmpeg -i water_medium.mp3 -ar 16000 -ac 1 -f s16le water_medium.raw │
 * │  xxd -i water_medium.raw > water_medium.h                             │
 * │                                                                        │
 * │  Rename array to: water_medium_pcm                                    │
 * │  Rename length to: water_medium_pcm_len                               │
 * └────────────────────────────────────────────────────────────────────────┘
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef WATER_MEDIUM_H
#define WATER_MEDIUM_H

#include <stdint.h>
#include <stddef.h>

extern const int16_t water_medium_pcm[];
extern const size_t  water_medium_pcm_len;

#endif /* WATER_MEDIUM_H */
