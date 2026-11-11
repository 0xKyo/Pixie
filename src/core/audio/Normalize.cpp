#include "core/audio/Normalize.h"

#include <algorithm>
#include <cmath>

namespace pixie::audio {

namespace {

constexpr float kSilentPeak = 1e-4f; // about -80 dBFS: scaling this would only amplify noise

float DbToAmplitude(float db) { return std::pow(10.0f, db / 20.0f); }

} // namespace

float PeakAmplitude(const AudioClip& clip) {
    float peak = 0.0f;
    for (const float sample : clip.samples) peak = std::max(peak, std::abs(sample));
    return peak;
}

bool IsPeakNormalized(const AudioClip& clip, float targetDb, float toleranceDb) {
    const float peak = PeakAmplitude(clip);
    if (peak < kSilentPeak) return true;
    return std::abs(20.0f * std::log10(peak / DbToAmplitude(targetDb))) < toleranceDb;
}

void NormalizePeak(AudioClip& clip, float targetDb) {
    const float peak = PeakAmplitude(clip);
    if (peak < kSilentPeak) return;
    const float gain = DbToAmplitude(targetDb) / peak;
    for (float& sample : clip.samples) sample *= gain;
}

} // namespace pixie::audio
