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

std::uint32_t ReadU32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint16_t ReadU16(const unsigned char* bytes) { return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8)); }

} // namespace

bool HasWavFormat(const std::filesystem::path& path, int channels, int sampleRate, int bitsPerSample) {
    std::vector<unsigned char> bytes;
    if (!formats::ReadWholeFile(path, bytes)) return false;
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return false;

    // Walk the chunks until "fmt "; the layout lives in its first 16 bytes (plus the sub-format for WAVE_FORMAT_EXTENSIBLE).
    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const size_t payloadSize = ReadU32(bytes.data() + offset + 4);
        const size_t payload = offset + 8;
        if (std::memcmp(bytes.data() + offset, "fmt ", 4) == 0) {
            if (payload + 16 > bytes.size()) return false;
            std::uint16_t formatTag = ReadU16(bytes.data() + payload);
            if (formatTag == 0xFFFE && payloadSize >= 26 && payload + 26 <= bytes.size()) formatTag = ReadU16(bytes.data() + payload + 24);
            return formatTag == 1 && ReadU16(bytes.data() + payload + 2) == channels &&
                   static_cast<int>(ReadU32(bytes.data() + payload + 4)) == sampleRate && ReadU16(bytes.data() + payload + 14) == bitsPerSample;
        }
        offset = payload + payloadSize + (payloadSize & 1); // chunks are padded to an even size
    }
    return false;
}

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
