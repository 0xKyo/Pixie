#pragma once

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// Every imported sound ends up stereo at 44.1 kHz. SFX are 16-bit PCM WAV; music is Ogg Vorbis.
inline constexpr int kStandardChannels = 2;
inline constexpr int kStandardSampleRate = 44100;
inline constexpr int kSfxBitsPerSample = 16;

// Mono is duplicated into both channels; with 3+ channels only the first two are kept.
[[nodiscard]] AudioClip ToStereo(const AudioClip& clip);

// Band-limited (windowed sinc) conversion; returns the clip unchanged if it already has that rate.
[[nodiscard]] AudioClip ResampleClip(const AudioClip& clip, int targetSampleRate);

// ToStereo followed by ResampleClip to kStandardSampleRate.
[[nodiscard]] AudioClip ConvertToStandardFormat(const AudioClip& clip);

} // namespace pixie::audio
