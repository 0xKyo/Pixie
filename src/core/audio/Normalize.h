#pragma once

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// Every SFX WAV is scaled so its loudest sample sits at this level (dBFS), leaving a little headroom.
inline constexpr float kSfxPeakDb = -1.0f;

// Largest absolute sample value (0 for an empty clip).
[[nodiscard]] float PeakAmplitude(const AudioClip& clip);

// True if the clip's peak is already within `toleranceDb` of `targetDb`, or it is silent (nothing to scale).
[[nodiscard]] bool IsPeakNormalized(const AudioClip& clip, float targetDb, float toleranceDb = 0.1f);

// Scales the whole clip (same gain on every channel) so its peak lands on `targetDb`; silent clips are left alone.
void NormalizePeak(AudioClip& clip, float targetDb);

} // namespace pixie::audio
