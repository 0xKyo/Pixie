#pragma once

#include <cstdint>

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// The longest crossfade that fits: at most half of the clip.
[[nodiscard]] std::uint64_t MaxLoopCrossfadeFrames(const AudioClip& clip);

// Returns a clip that is `crossfadeFrames` shorter whose end flows into its start; the tail is blended into the head.
[[nodiscard]] AudioClip MakeLoopable(const AudioClip& clip, std::uint64_t crossfadeFrames);

} // namespace pixie::audio
