#include "core/audio/SilenceTrim.h"

#include "core/audio/OggWrite.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/formats/BinaryFile.h"

namespace pixie::audio {

namespace {

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

std::uint16_t ReadU16(const unsigned char* bytes) { return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8)); }

std::uint32_t ReadU32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void WriteU32(std::vector<unsigned char>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<unsigned char>((value >> shift) & 0xFF));
}

// One chunk of a RIFF file: where its payload starts and how long it is.
struct Chunk {
    char id[4] = {};
    size_t payloadOffset = 0;
    size_t payloadSize = 0;
};

// Splits the bytes after "RIFF....WAVE" into chunks; a data chunk that claims more than the file holds is clamped.
std::vector<Chunk> ReadChunks(const std::vector<unsigned char>& bytes) {
    std::vector<Chunk> chunks;
    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        Chunk chunk;
        std::memcpy(chunk.id, bytes.data() + offset, 4);
        chunk.payloadOffset = offset + 8;
        chunk.payloadSize = std::min<size_t>(ReadU32(bytes.data() + offset + 4), bytes.size() - chunk.payloadOffset);
        chunks.push_back(chunk);
        offset = chunk.payloadOffset + chunk.payloadSize + (chunk.payloadSize & 1); // chunks are padded to even sizes
    }
    return chunks;
}

bool HasId(const Chunk& chunk, const char* id) { return std::memcmp(chunk.id, id, 4) == 0; }

std::string LowercaseExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return extension;
}

} // namespace

bool FindSoundRange(const AudioClip& clip, const SilenceSettings& settings, FrameRange& range) {
    const std::uint64_t frameCount = clip.FrameCount();
    const size_t channels = static_cast<size_t>(clip.channels);
    if (frameCount == 0) return false;

    auto isAudible = [&](std::uint64_t frame) {
        for (size_t channel = 0; channel < channels; ++channel) {
            if (std::fabs(clip.samples[frame * channels + channel]) > settings.thresholdAmplitude) return true;
        }
        return false;
    };

    std::uint64_t firstSound = 0;
    while (firstSound < frameCount && !isAudible(firstSound)) ++firstSound;
    if (firstSound == frameCount) return false; // nothing but silence

    std::uint64_t lastSound = frameCount - 1;
    while (lastSound > firstSound && !isAudible(lastSound)) --lastSound;

    const std::uint64_t padding = static_cast<std::uint64_t>(settings.paddingMs) * clip.sampleRate / 1000;
    range.firstFrame = firstSound > padding ? firstSound - padding : 0;
    range.endFrame = std::min(frameCount, lastSound + 1 + padding);
    return true;
}

bool CanTrim(const std::filesystem::path& path) {
    const std::string extension = LowercaseExtension(path);
    return extension == ".wav" || extension == ".ogg";
}

bool TrimWavFile(const std::filesystem::path& path, const FrameRange& keep, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };

    std::vector<unsigned char> bytes;
    if (!formats::ReadWholeFile(path, bytes, error)) return false;
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return fail("Not a RIFF/WAVE file");
    }

    const std::vector<Chunk> chunks = ReadChunks(bytes);
    const Chunk* format = nullptr;
    const Chunk* data = nullptr;
    for (const Chunk& chunk : chunks) {
        if (!format && HasId(chunk, "fmt ")) format = &chunk;
        if (!data && HasId(chunk, "data")) data = &chunk;
    }
    if (!format || !data || format->payloadSize < 16) return fail("The WAV file has no readable format or data");

    const std::uint16_t formatTag = ReadU16(bytes.data() + format->payloadOffset);
    if (formatTag != kFormatPcm && formatTag != kFormatFloat && formatTag != kFormatExtensible) {
        return fail("Only PCM and float WAV files can be trimmed");
    }
    const std::uint16_t bytesPerFrame = ReadU16(bytes.data() + format->payloadOffset + 12);
    if (bytesPerFrame == 0) return fail("The WAV file has an invalid block size");

    const std::uint64_t frameCount = data->payloadSize / bytesPerFrame;
    if (keep.endFrame > frameCount || keep.firstFrame >= keep.endFrame) return fail("The range to keep doesn't match the file");

    // Rebuild the file chunk by chunk, swapping only the sample data.
    std::vector<unsigned char> output = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    for (const Chunk& chunk : chunks) {
        const bool isData = &chunk == data;
        const size_t payloadSize = isData ? static_cast<size_t>(keep.Length() * bytesPerFrame) : chunk.payloadSize;
        const size_t payloadStart = chunk.payloadOffset + (isData ? static_cast<size_t>(keep.firstFrame * bytesPerFrame) : 0);

        output.insert(output.end(), chunk.id, chunk.id + 4);
        WriteU32(output, static_cast<std::uint32_t>(payloadSize));
        output.insert(output.end(), bytes.begin() + payloadStart, bytes.begin() + payloadStart + payloadSize);
        if (payloadSize & 1) output.push_back(0);
    }
    const std::uint32_t riffSize = static_cast<std::uint32_t>(output.size() - 8);
    for (int i = 0; i < 4; ++i) output[4 + i] = static_cast<unsigned char>((riffSize >> (8 * i)) & 0xFF);

    // Write next to the original first, so a failed write never leaves a half-written audio file.
    std::filesystem::path temporary = path;
    temporary += ".trimming";
    if (!formats::WriteWholeFile(temporary, output.data(), output.size(), error)) return false;
    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError); // replaces the original on Windows and POSIX
    if (renameError) {
        std::filesystem::remove(temporary, renameError);
        return fail("Could not replace the original file");
    }
    return true;
}

bool TrimOggFile(const std::filesystem::path& path, const FrameRange& keep, std::string* error) {
    AudioClip clip;
    if (!LoadClip(path, clip, error)) return false;
    if (keep.endFrame <= keep.firstFrame || keep.endFrame > clip.FrameCount()) {
        if (error) *error = "the range to keep is invalid";
        return false;
    }

    AudioClip trimmed = clip;
    const auto channels = static_cast<std::ptrdiff_t>(clip.channels);
    trimmed.samples.assign(clip.samples.begin() + static_cast<std::ptrdiff_t>(keep.firstFrame) * channels,
                           clip.samples.begin() + static_cast<std::ptrdiff_t>(keep.endFrame) * channels);

    // Write beside the original first so a failed encode never damages it.
    std::filesystem::path temporary = path;
    temporary += ".trimming";
    std::error_code fileError;
    if (!SaveOgg(trimmed, temporary, kMusicOggQuality, error)) {
        std::filesystem::remove(temporary, fileError);
        return false;
    }
    std::filesystem::rename(temporary, path, fileError);
    if (fileError) {
        if (error) *error = "could not replace the file: " + fileError.message();
        std::filesystem::remove(temporary, fileError);
        return false;
    }
    return true;
}

bool TrimAudioFile(const std::filesystem::path& path, const FrameRange& keep, std::string* error) {
    return LowercaseExtension(path) == ".ogg" ? TrimOggFile(path, keep, error) : TrimWavFile(path, keep, error);
}

} // namespace pixie::audio
