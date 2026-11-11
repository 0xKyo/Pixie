#include "core/audio/Resample.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pixie::audio {

namespace {

constexpr int kZeroCrossings = 16; // quality of the low-pass filter: sinc lobes kept on each side

double Sinc(double x) {
    if (std::abs(x) < 1e-9) return 1.0;
    const double angle = std::numbers::pi * x;
    return std::sin(angle) / angle;
}

} // namespace

AudioClip ToStereo(const AudioClip& clip) {
    if (clip.Empty() || clip.channels == kStandardChannels) return clip;

    AudioClip stereo;
    stereo.channels = kStandardChannels;
    stereo.sampleRate = clip.sampleRate;
    stereo.codec = clip.codec;
    const size_t frames = static_cast<size_t>(clip.FrameCount());
    stereo.samples.resize(frames * kStandardChannels);
    const size_t rightChannel = clip.channels > 1 ? 1 : 0;
    for (size_t frame = 0; frame < frames; ++frame) {
        stereo.samples[frame * 2] = clip.samples[frame * clip.channels];
        stereo.samples[frame * 2 + 1] = clip.samples[frame * clip.channels + rightChannel];
    }
    return stereo;
}

AudioClip ResampleClip(const AudioClip& clip, int targetSampleRate) {
    if (clip.Empty() || targetSampleRate <= 0 || clip.sampleRate == targetSampleRate) return clip;

    const double ratio = static_cast<double>(targetSampleRate) / clip.sampleRate;
    const double inputStep = 1.0 / ratio;                   // input frames per output frame
    const double cutoff = std::min(1.0, ratio);             // fraction of the input's Nyquist; lower when shrinking the rate
    const double radius = kZeroCrossings / cutoff;          // kernel half-width in input frames
    const int64_t inputFrames = static_cast<int64_t>(clip.FrameCount());
    const int64_t outputFrames = std::max<int64_t>(1, std::llround(static_cast<double>(inputFrames) * ratio));

    AudioClip resampled;
    resampled.channels = clip.channels;
    resampled.sampleRate = targetSampleRate;
    resampled.codec = clip.codec;
    resampled.samples.assign(static_cast<size_t>(outputFrames) * clip.channels, 0.0f);

    std::vector<double> sums(static_cast<size_t>(clip.channels));
    for (int64_t outputFrame = 0; outputFrame < outputFrames; ++outputFrame) {
        const double center = static_cast<double>(outputFrame) * inputStep;
        const int64_t first = std::max<int64_t>(0, static_cast<int64_t>(std::ceil(center - radius)));
        const int64_t last = std::min<int64_t>(inputFrames - 1, static_cast<int64_t>(std::floor(center + radius)));
        std::fill(sums.begin(), sums.end(), 0.0);
        for (int64_t inputFrame = first; inputFrame <= last; ++inputFrame) {
            const double offset = static_cast<double>(inputFrame) - center;
            const double window = 0.5 + 0.5 * std::cos(std::numbers::pi * offset / radius); // Hann
            const double weight = cutoff * Sinc(cutoff * offset) * window;
            for (int channel = 0; channel < clip.channels; ++channel) {
                sums[channel] += weight * clip.samples[static_cast<size_t>(inputFrame) * clip.channels + channel];
            }
        }
        for (int channel = 0; channel < clip.channels; ++channel) {
            resampled.samples[static_cast<size_t>(outputFrame) * clip.channels + channel] = static_cast<float>(sums[channel]);
        }
    }
    return resampled;
}

AudioClip ConvertToStandardFormat(const AudioClip& clip) { return ResampleClip(ToStereo(clip), kStandardSampleRate); }

} // namespace pixie::audio
