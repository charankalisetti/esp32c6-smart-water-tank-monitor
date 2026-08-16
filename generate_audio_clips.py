#!/usr/bin/env python3
"""
generate_audio_clips.py
=======================
Generates real TTS speech for the Water Tank Level Monitor and writes
a complete audio_clips.c with properly formatted int16_t PCM arrays.

Speech clips generated:
  "Tank Empty"
  "Water Level Low"
  "Water Level Sixty One Percent"
  "Tank Full"

Audio format (matches audio_player.h):
  Sample rate : 16000 Hz
  Bit depth   : 16-bit signed PCM
  Channels    : Mono

MP3 decode strategy (tried in order, no ffmpeg required for option 1):
  1. miniaudio  — pure-Python MP3 decoder, zero system dependencies
  2. pydub      — requires ffmpeg on PATH (fallback)

Usage:
  python generate_audio_clips.py

Output:
  main/audio/audio_clips.c   <- Replaces the placeholder file

Author: Senior ESP-IDF Embedded Systems Engineer
"""

import sys
import os
import struct
import subprocess
import io
import tempfile
from pathlib import Path
from datetime import datetime

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SAMPLE_RATE  = 16000    # Must match AUDIO_SAMPLE_RATE_HZ in audio_player.h
CHANNELS     = 1        # Mono
SAMPLE_WIDTH = 2        # 16-bit = 2 bytes

SCRIPT_DIR  = Path(__file__).parent
OUTPUT_FILE = SCRIPT_DIR / "components" / "drivers" / "audio" / "audio_clips.c"

# All voice clips to generate for the firmware
CLIPS = [
    # English clips
    ("tank_empty",           "Tank Empty",                                                                                                      "Tank Empty",                    "en"),
    ("water_low",            "Water Level Low",                                                                                                 "Water Level Low",               "en"),
    ("water_medium",         "Water Level Sixty One Percent",                                                                                   "Water Level Sixty One Percent", "en"),
    ("tank_full",            "Tank Full",                                                                                                       "Tank Full",                     "en"),
    ("fault_probe_low",      "Warning: Low water sensor probe 20 centimeter fault. Wire is disconnected or corroded. Please check probe one.", "Fault Probe Low 20cm",          "en"),
    ("fault_probe_med",      "Warning: Medium water sensor probe 55 centimeter fault. Wire is disconnected or corroded. Please check probe two.", "Fault Probe Med 55cm",          "en"),
    ("fault_probe_general",  "Warning: Water sensor wiring fault. Please check sensor probes.",                                                 "Fault Probe General",           "en"),

    # Telugu diagnostic fault clips
    ("fault_probe_low_te",     "హెచ్చరిక: 20 సెంటీమీటర్ల దిగువ నీటి సెన్సార్ పనిచేయడం లేదు. దయచేసి మొదటి వైరును తనిఖీ చేయండి.",                  "హెచ్చరిక: 20సెం.మీ దిగువ సెన్సార్ లోపం", "te"),
    ("fault_probe_med_te",     "హెచ్చరిక: 55 సెంటీమీటర్ల మధ్య నీటి సెన్సార్ పనిచేయడం లేదు. దయచేసి రెండవ వైరును తనిఖీ చేయండి.",                  "హెచ్చరిక: 55సెం.మీ మధ్య సెన్సార్ లోపం",  "te"),
    ("fault_probe_general_te", "హెచ్చరిక: వాటర్ సెన్సార్ వైరింగ్ లోపం. దయచేసి కనెక్షన్లను తనిఖీ చేయండి.",                                            "హెచ్చరిక: వైరింగ్ లోపం",               "te"),
]

# ---------------------------------------------------------------------------
# Dependency bootstrap
# ---------------------------------------------------------------------------

def pip_install(*packages):
    subprocess.run(
        [sys.executable, "-m", "pip", "install", "--quiet", *packages],
        check=True
    )

def ensure_deps():
    print("[setup] Installing Python dependencies...")
    pip_install("gtts", "miniaudio", "numpy")
    # pydub only needed as fallback; install but do not abort if it fails
    try:
        pip_install("pydub")
    except Exception:
        pass
    print("[setup] Done.\n")

# ---------------------------------------------------------------------------
# MP3 → raw 16-bit mono PCM bytes
# ---------------------------------------------------------------------------

def mp3_bytes_to_pcm_miniaudio(mp3_bytes: bytes) -> bytes:
    """
    Decode MP3 bytes to 16-bit mono PCM at SAMPLE_RATE Hz using miniaudio.
    miniaudio is a pure-Python wrapper around the miniaudio C library —
    no ffmpeg, no system binary required.
    """
    import miniaudio

    with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as tmp:
        tmp.write(mp3_bytes)
        tmp_path = tmp.name

    try:
        decoded = miniaudio.decode_file(
            tmp_path,
            output_format=miniaudio.SampleFormat.SIGNED16,
            nchannels=CHANNELS,
            sample_rate=SAMPLE_RATE,
        )
        return bytes(decoded.samples)
    finally:
        os.unlink(tmp_path)


def mp3_bytes_to_pcm_pydub(mp3_bytes: bytes) -> bytes:
    """
    Fallback: decode MP3 using pydub (requires ffmpeg on PATH).
    """
    from pydub import AudioSegment

    buf   = io.BytesIO(mp3_bytes)
    audio = AudioSegment.from_file(buf, format="mp3")
    audio = audio.set_frame_rate(SAMPLE_RATE)
    audio = audio.set_channels(CHANNELS)
    audio = audio.set_sample_width(SAMPLE_WIDTH)
    return audio.raw_data


def mp3_to_pcm(mp3_bytes: bytes) -> bytes:
    """Try miniaudio first, fall back to pydub."""
    try:
        import miniaudio
        return mp3_bytes_to_pcm_miniaudio(mp3_bytes)
    except Exception as e:
        print(f"       [miniaudio failed: {e}] — falling back to pydub/ffmpeg")
        return mp3_bytes_to_pcm_pydub(mp3_bytes)


# ---------------------------------------------------------------------------
# TTS generation
# ---------------------------------------------------------------------------

def tts_to_pcm(phrase: str, lang: str = "en") -> bytes:
    """Generate speech for phrase and return 16-bit mono PCM bytes."""
    from gtts import gTTS

    print(f"       Fetching TTS from Google (lang={lang})...")
    buf = io.BytesIO()
    gTTS(text=phrase, lang=lang, slow=False).write_to_fp(buf)
    mp3_bytes = buf.getvalue()
    print(f"       MP3 size: {len(mp3_bytes):,} bytes — decoding...")
    return mp3_to_pcm(mp3_bytes)


def pcm_bytes_to_int16(raw: bytes) -> list:
    count = len(raw) // SAMPLE_WIDTH
    return list(struct.unpack(f"<{count}h", raw[:count * SAMPLE_WIDTH]))


# Target peak amplitude: 90% of int16 max (32767) = 29490
# Leaves 10% headroom to avoid hard clipping on the MAX98357A output stage.
NORMALIZE_TARGET = 0.90

def normalize_pcm(samples: list) -> list:
    """Scale PCM samples so the peak reaches NORMALIZE_TARGET * 32767.

    Google TTS clips typically peak at 40-50% of full scale.
    Normalizing to 90% recovers ~6 dB of lost loudness without clipping.
    """
    if not samples:
        return samples
    peak = max(abs(s) for s in samples)
    if peak == 0:
        return samples
    scale = (NORMALIZE_TARGET * 32767.0) / peak
    normalized = [max(-32768, min(32767, int(s * scale))) for s in samples]
    actual_peak = max(abs(s) for s in normalized)
    print(f"       Normalized: peak {peak} -> {actual_peak} "
          f"(scale x{scale:.3f}, {20 * __import__('math').log10(scale):.1f} dB boost)")
    return normalized


# ---------------------------------------------------------------------------
# C file generation
# ---------------------------------------------------------------------------

COLS = 8   # int16 values per line

FILE_HEADER = """\
/**
 * @file audio_clips.c
 * @brief Real TTS PCM audio for the Water Tank Level Monitor.
 *
 * AUTO-GENERATED by generate_audio_clips.py on {timestamp}
 * DO NOT EDIT BY HAND — re-run the script to regenerate.
 *
 * Source  : Google Text-to-Speech
 * Format  : {sample_rate} Hz, 16-bit signed, Mono
 *
 * Clips:
{clip_list}
 *
 * To regenerate:
 *   python generate_audio_clips.py
 *
 * @author Senior ESP-IDF Embedded Systems Engineer
 */

#include <stdint.h>
#include <stddef.h>

"""


def c_array(name: str, samples: list) -> str:
    rows = []
    for i in range(0, len(samples), COLS):
        chunk = samples[i:i + COLS]
        rows.append("    " + ", ".join(f"{v:7d}" for v in chunk) + ",")
    body = "\n".join(rows)
    return (
        f"const int16_t {name}_pcm[] = {{\n"
        f"{body}\n"
        f"}};\n"
        f"const size_t {name}_pcm_len ="
        f" sizeof({name}_pcm) / sizeof({name}_pcm[0]);\n"
    )


def build_c_file(clip_data):
    clip_list = "\n".join(
        f" *   {name}_pcm  \"{label}\"  ({len(s)/SAMPLE_RATE:.2f} s)"
        for name, s, label, _ in clip_data
    )
    header = FILE_HEADER.format(
        timestamp   = datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        sample_rate = SAMPLE_RATE,
        clip_list   = clip_list,
    )
    parts = []
    for name, samples, label, _ in clip_data:
        sep = (
            f"/* -------------------------------------------------------------------\n"
            f" * \"{label}\"\n"
            f" * {len(samples):,} samples  |  {len(samples)/SAMPLE_RATE:.2f} s\n"
            f" * ------------------------------------------------------------------- */\n"
        )
        parts.append(sep + c_array(name, samples))
    return header + "\n".join(parts) + "\n"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    print("=" * 60)
    print(" Water Tank Monitor - Audio Clip Generator")
    print(f" Output -> {OUTPUT_FILE}")
    print("=" * 60)
    print()

    ensure_deps()

    clip_data = []
    for c_name, phrase, label, lang in CLIPS:
        print(f"[tts]  {c_name} (lang={lang})")
        raw = tts_to_pcm(phrase, lang=lang)
        samples = pcm_bytes_to_int16(raw)
        samples = normalize_pcm(samples)
        print(f"       -> {len(samples):,} samples  ({len(samples)/SAMPLE_RATE:.2f} s)\n")
        clip_data.append((c_name, samples, label, lang))

    print("[write] Generating audio_clips.c ...")
    c_src = build_c_file(clip_data)
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_FILE.write_text(c_src, encoding="utf-8")

    total_samples = sum(len(s) for _, s, _, _ in clip_data)
    total_kb = total_samples * SAMPLE_WIDTH / 1024
    print(f"[done]  Wrote {OUTPUT_FILE.name}")
    print(f"        Total flash usage: ~{total_kb:.1f} KB "
          f"({total_samples:,} samples across {len(clip_data)} clips)\n")

    print("-" * 60)
    print("IMPORTANT: Open audio_player.h and set:")
    print("  #define AUDIO_CLIP_LOOP_COUNT   1u")
    print("Real clips are full-length — looping is not needed.")
    print("-" * 60)


if __name__ == "__main__":
    main()
