#pragma once

#include <filesystem>
#include <string>

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// Writes the clip as a 16-bit PCM .wav file.
bool SaveWav(const AudioClip& clip, const std::filesystem::path& path, std::string* error = nullptr);

// True if the file is an integer-PCM .wav with exactly this layout (reads only the header chunks).
[[nodiscard]] bool HasWavFormat(const std::filesystem::path& path, int channels, int sampleRate, int bitsPerSample);

} // namespace pixie::audio
