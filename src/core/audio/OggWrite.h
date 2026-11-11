#pragma once

#include <filesystem>
#include <string>

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// Vorbis VBR quality used for all music (about 160 kbps).
inline constexpr float kMusicOggQuality = 0.5f;

// True if the file is Ogg Vorbis with exactly this layout (reads only the first bytes of the file).
[[nodiscard]] bool HasOggFormat(const std::filesystem::path& path, int channels, int sampleRate);

// Writes the clip as an Ogg Vorbis file; `quality` is the Vorbis VBR quality from -0.1 to 1.0 (0.5 is about 160 kbps).
bool SaveOgg(const AudioClip& clip, const std::filesystem::path& path, float quality = 0.5f, std::string* error = nullptr);

} // namespace pixie::audio
