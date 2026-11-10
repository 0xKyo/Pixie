#pragma once

#include <filesystem>
#include <string>

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// Writes the clip as a 16-bit PCM .wav file.
bool SaveWav(const AudioClip& clip, const std::filesystem::path& path, std::string* error = nullptr);

} // namespace pixie::audio
