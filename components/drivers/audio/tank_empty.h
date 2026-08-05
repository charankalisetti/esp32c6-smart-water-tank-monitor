/**
 * @file tank_empty.h
 * @brief Embedded PCM audio — "Tank Empty" announcement.
 *
 * Format : 16-bit signed PCM, Mono, 16000 Hz sample rate
 * Content: Synthesized placeholder waveform.
 *
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │  REPLACEMENT INSTRUCTIONS                                              │
 * │                                                                        │
 * │  Record or synthesize "Tank Empty" speech at 16000 Hz, Mono, 16-bit.  │
 * │  Then convert to a raw PCM array using:                               │
 * │                                                                        │
 * │  ffmpeg -i tank_empty.mp3 -ar 16000 -ac 1 -f s16le tank_empty.raw    │
 * │                                                                        │
 * │  Then convert the .raw file to this C header format using:            │
 * │                                                                        │
 * │  xxd -i tank_empty.raw > tank_empty.h                                 │
 * │                                                                        │
 * │  Rename the array to: tank_empty_pcm                                  │
 * │  Rename the length to: tank_empty_pcm_len                             │
 * └────────────────────────────────────────────────────────────────────────┘
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#ifndef TANK_EMPTY_H
#define TANK_EMPTY_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Raw 16-bit PCM audio samples for "Tank Empty" announcement.
 *
 * Stored in flash (.rodata) — never copied to RAM.
 * Sample rate : 16000 Hz
 * Bit depth   : 16-bit signed
 * Channels    : 1 (Mono)
 * Duration    : ~0.5 s placeholder (8000 samples)
 *
 * This placeholder generates a brief 800 Hz tone burst shaped to fade in/out,
 * which will play audibly through the MAX98357A to confirm the audio pipeline
 * works before real speech audio is substituted.
 */
extern const int16_t tank_empty_pcm[];
extern const size_t  tank_empty_pcm_len; /* Number of int16_t samples */

#endif /* TANK_EMPTY_H */
