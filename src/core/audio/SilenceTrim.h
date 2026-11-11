#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/audio/AudioClip.h"

namespace pixie::audio {

// A run of sample frames: [firstFrame, endFrame).
struct FrameRange {
    std::uint64_t firstFrame = 0;
    std::uint64_t endFrame = 0;

    [[nodiscard]] std::uint64_t Length() const { return endFrame - firstFrame; }
};

struct SilenceSettings {
    float thresholdAmplitude = 0.003f; // about -50 dBFS: anything quieter counts as silence
    int paddingMs = 5;                 // quiet margin kept around the sound so cuts don't click
};

// The part of the clip from its first to its last audible sample, plus padding; false if it's all silence.
bool FindSoundRange(const AudioClip& clip, const SilenceSettings& settings, FrameRange& range);

// WAV files can be trimmed (samples are copied byte for byte, so quality never changes) and Ogg files too (decoded,
// cut and re-encoded). The same two formats can be made loopable, since both are rewritten from decoded samples.
[[nodiscard]] bool CanTrim(const std::filesystem::path& path);

// Rewrites the WAV file keeping only `keep`; the sample format and any other chunks stay as they were.
bool TrimWavFile(const std::filesystem::path& path, const FrameRange& keep, std::string* error = nullptr);

// Decodes the Ogg file, keeps only `keep` and re-encodes it over the original (a second lossy pass).
bool TrimOggFile(const std::filesystem::path& path, const FrameRange& keep, std::string* error = nullptr);

// TrimWavFile for .wav, TrimOggFile for .ogg.
bool TrimAudioFile(const std::filesystem::path& path, const FrameRange& keep, std::string* error = nullptr);

} // namespace pixie::audio
