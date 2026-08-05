/**
 * @file tank_full.h
 * @brief Embedded PCM audio — "Tank Full" announcement.
 *
 * Format : 16-bit signed PCM, Mono, 16000 Hz sample rate
 * Content: Synthesized placeholder waveform.
 *
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │  REPLACEMENT INSTRUCTIONS                                              │
 * │                                                                        │
 * │  Record or synthesize "Tank Full" at 16000 Hz, Mono, 16-bit.         │
 * │                                                                        │
 * │  ffmpeg -i tank_full.mp3 -ar 16000 -ac 1 -f s16le tank_full.raw      │
 * │  xxd -i tank_full.raw > tank_full.h                                   │
 * │                                                                        │
 * │  Rename array to: tank_full_pcm                                       │
 * │  Rename length to: tank_full_pcm_len                                  │
 * └────────────────────────────────────────────────────────────────────────┘
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef TANK_FULL_H
#define TANK_FULL_H

#include <stdint.h>
#include <stddef.h>

extern const int16_t tank_full_pcm[];
extern const size_t  tank_full_pcm_len;

#endif /* TANK_FULL_H */
