#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pixie::audio {

// A fully decoded sound: interleaved float samples in [-1, 1].
struct AudioClip {
    std::vector<float> samples;
    int channels = 0;
    int sampleRate = 0;
    std::string codec; // "WAV", "MP3", "FLAC" or "Ogg Vorbis"

    [[nodiscard]] bool Empty() const { return channels <= 0 || samples.empty(); }
    [[nodiscard]] uint64_t FrameCount() const { return channels > 0 ? samples.size() / static_cast<size_t>(channels) : 0; }
    [[nodiscard]] double DurationSeconds() const { return sampleRate > 0 ? static_cast<double>(FrameCount()) / sampleRate : 0.0; }
};

// True for the formats Pixie can decode (.wav, .mp3, .flac, .ogg); others are listed but not previewed.
[[nodiscard]] bool CanDecode(const std::filesystem::path& path);

bool LoadClip(const std::filesystem::path& path, AudioClip& clip, std::string* error = nullptr);

// Lowest and highest sample (mixed to mono) inside one horizontal slice of a waveform.
struct WaveformColumn {
    float min = 0.0f;
    float max = 0.0f;
};

// Splits the clip into `columnCount` slices for drawing its waveform.
std::vector<WaveformColumn> ComputeWaveform(const AudioClip& clip, int columnCount);

} // namespace pixie::audio
