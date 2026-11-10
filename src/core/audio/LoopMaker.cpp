#include "core/audio/LoopMaker.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pixie::audio {

std::uint64_t MaxLoopCrossfadeFrames(const AudioClip& clip) { return clip.FrameCount() / 2; }

AudioClip MakeLoopable(const AudioClip& clip, std::uint64_t crossfadeFrames) {
    crossfadeFrames = std::min(crossfadeFrames, MaxLoopCrossfadeFrames(clip));
    if (clip.Empty() || crossfadeFrames == 0) return clip;

    const size_t channels = static_cast<size_t>(clip.channels);
    const std::uint64_t keptFrames = clip.FrameCount() - crossfadeFrames;
    AudioClip result = clip;
    result.samples.assign(clip.samples.begin(), clip.samples.begin() + static_cast<std::ptrdiff_t>(keptFrames * channels));

    // The first frames become a blend of the cut-off tail (fading out) and the original head (fading in).
    for (std::uint64_t frame = 0; frame < crossfadeFrames; ++frame) {
        const double position = (static_cast<double>(frame) + 0.5) / static_cast<double>(crossfadeFrames);
        const float headGain = static_cast<float>(std::sin(position * std::numbers::pi / 2.0)); // equal power keeps the loudness steady
        const float tailGain = static_cast<float>(std::cos(position * std::numbers::pi / 2.0));
        for (size_t channel = 0; channel < channels; ++channel) {
            const float head = clip.samples[frame * channels + channel];
            const float tail = clip.samples[(keptFrames + frame) * channels + channel];
            result.samples[frame * channels + channel] = std::clamp(head * headGain + tail * tailGain, -1.0f, 1.0f);
        }
    }
    return result;
}

} // namespace pixie::audio
