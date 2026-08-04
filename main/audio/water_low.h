/**
 * @file water_low.h
 * @brief Embedded PCM audio — "Water Level Low" announcement.
 *
 * Format : 16-bit signed PCM, Mono, 16000 Hz sample rate
 * Content: Synthesized placeholder waveform.
 *
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │  REPLACEMENT INSTRUCTIONS                                              │
 * │                                                                        │
 * │  Record or synthesize "Water Level Low" at 16000 Hz, Mono, 16-bit.   │
 * │                                                                        │
 * │  ffmpeg -i water_low.mp3 -ar 16000 -ac 1 -f s16le water_low.raw      │
 * │  xxd -i water_low.raw > water_low.h                                   │
 * │                                                                        │
 * │  Rename array to: water_low_pcm                                       │
 * │  Rename length to: water_low_pcm_len                                  │
 * └────────────────────────────────────────────────────────────────────────┘
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef WATER_LOW_H
#define WATER_LOW_H

#include <stdint.h>
#include <stddef.h>

extern const int16_t water_low_pcm[];
extern const size_t  water_low_pcm_len;

#endif /* WATER_LOW_H */
