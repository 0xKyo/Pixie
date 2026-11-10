#include "core/audio/WavWrite.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/formats/BinaryFile.h"

namespace pixie::audio {

namespace {

void PutU16(std::vector<unsigned char>& out, std::uint16_t value) {
    out.push_back(static_cast<unsigned char>(value & 0xFF));
    out.push_back(static_cast<unsigned char>(value >> 8));
}

void PutU32(std::vector<unsigned char>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<unsigned char>((value >> shift) & 0xFF));
}

void PutTag(std::vector<unsigned char>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

} // namespace

bool SaveWav(const AudioClip& clip, const std::filesystem::path& path, std::string* error) {
    if (clip.Empty()) {
        if (error) *error = "the clip has no samples";
        return false;
    }
    const std::uint16_t channels = static_cast<std::uint16_t>(clip.channels);
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(clip.samples.size() * sizeof(std::int16_t));
    const std::uint32_t byteRate = static_cast<std::uint32_t>(clip.sampleRate) * channels * sizeof(std::int16_t);

    std::vector<unsigned char> bytes;
    bytes.reserve(44 + dataBytes);
    PutTag(bytes, "RIFF");
    PutU32(bytes, 36 + dataBytes);
    PutTag(bytes, "WAVE");
    PutTag(bytes, "fmt ");
    PutU32(bytes, 16);
    PutU16(bytes, 1); // PCM
    PutU16(bytes, channels);
    PutU32(bytes, static_cast<std::uint32_t>(clip.sampleRate));
    PutU32(bytes, byteRate);
    PutU16(bytes, static_cast<std::uint16_t>(channels * sizeof(std::int16_t)));
    PutU16(bytes, 16);
    PutTag(bytes, "data");
    PutU32(bytes, dataBytes);
    for (const float sample : clip.samples) {
        const float clamped = std::clamp(sample, -1.0f, 1.0f);
        PutU16(bytes, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(clamped * 32767.0f))));
    }
    return formats::WriteWholeFile(path, bytes.data(), bytes.size(), error);
}

} // namespace pixie::audio
