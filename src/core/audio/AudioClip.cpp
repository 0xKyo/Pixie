#include "core/audio/AudioClip.h"

// miniaudio is only used to decode files, so all playback/engine parts are compiled out.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <cstdlib>

#include "core/formats/BinaryFile.h"

// Defined in StbVorbis.cpp (stb_vorbis declares it with C linkage).
extern "C" int stb_vorbis_decode_memory(const unsigned char* data, int size, int* channels, int* sampleRate, short** samples);

namespace pixie::audio {

namespace {

std::string LowercaseExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& c : extension) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return extension;
}

bool DecodeOggVorbis(const std::vector<unsigned char>& bytes, AudioClip& clip) {
    int channels = 0, sampleRate = 0;
    short* samples = nullptr;
    const int frameCount = stb_vorbis_decode_memory(bytes.data(), static_cast<int>(bytes.size()), &channels, &sampleRate, &samples);
    if (frameCount <= 0 || !samples) return false;

    clip.channels = channels;
    clip.sampleRate = sampleRate;
    clip.codec = "Ogg Vorbis";
    clip.samples.resize(static_cast<size_t>(frameCount) * channels);
    for (size_t i = 0; i < clip.samples.size(); ++i) {
        clip.samples[i] = static_cast<float>(samples[i]) / 32768.0f;
    }
    std::free(samples);
    return true;
}

// WAV, MP3 and FLAC go through miniaudio, keeping the file's own channel count and sample rate.
bool DecodeWithMiniaudio(const std::vector<unsigned char>& bytes, const std::string& extension, AudioClip& clip) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;
    if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) != MA_SUCCESS) return false;

    clip.channels = static_cast<int>(decoder.outputChannels);
    clip.sampleRate = static_cast<int>(decoder.outputSampleRate);
    clip.codec = extension == ".wav" ? "WAV" : extension == ".mp3" ? "MP3" : "FLAC";

    ma_uint64 knownLength = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &knownLength) == MA_SUCCESS && knownLength > 0) {
        clip.samples.reserve(static_cast<size_t>(knownLength) * clip.channels);
    }

    constexpr ma_uint64 kFramesPerRead = 16384;
    std::vector<float> chunk(kFramesPerRead * static_cast<size_t>(clip.channels));
    for (;;) {
        ma_uint64 framesRead = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kFramesPerRead, &framesRead);
        clip.samples.insert(clip.samples.end(), chunk.begin(), chunk.begin() + static_cast<size_t>(framesRead) * clip.channels);
        if (result != MA_SUCCESS || framesRead == 0) break;
    }
    ma_decoder_uninit(&decoder);
    return true;
}

} // namespace

bool CanDecode(const std::filesystem::path& path) {
    const std::string extension = LowercaseExtension(path);
    return extension == ".wav" || extension == ".mp3" || extension == ".flac" || extension == ".ogg" || extension == ".oga";
}

bool LoadClip(const std::filesystem::path& path, AudioClip& clip, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    if (!CanDecode(path)) return fail("Preview isn't supported for " + path.extension().string() + " files");

    std::vector<unsigned char> bytes;
    if (!formats::ReadWholeFile(path, bytes, error)) return false;

    const std::string extension = LowercaseExtension(path);
    AudioClip decoded;
    const bool isOgg = extension == ".ogg" || extension == ".oga";
    const bool ok = isOgg ? DecodeOggVorbis(bytes, decoded) : DecodeWithMiniaudio(bytes, extension, decoded);
    if (!ok) return fail(isOgg ? "Could not decode Ogg Vorbis data" : "Could not decode the audio data");
    if (decoded.Empty() || decoded.sampleRate <= 0) return fail("No audio data found");

    clip = std::move(decoded);
    return true;
}

std::vector<WaveformColumn> ComputeWaveform(const AudioClip& clip, int columnCount) {
    std::vector<WaveformColumn> columns(static_cast<size_t>(std::max(1, columnCount)));
    const uint64_t frameCount = clip.FrameCount();
    if (frameCount == 0) return columns;

    const int channels = clip.channels;
    for (size_t column = 0; column < columns.size(); ++column) {
        const uint64_t firstFrame = frameCount * column / columns.size();
        const uint64_t endFrame = std::max(firstFrame + 1, frameCount * (column + 1) / columns.size());
        float lowest = 0.0f, highest = 0.0f;
        for (uint64_t frame = firstFrame; frame < endFrame && frame < frameCount; ++frame) {
            float sum = 0.0f;
            for (int channel = 0; channel < channels; ++channel) sum += clip.samples[frame * channels + channel];
            const float mono = sum / static_cast<float>(channels);
            lowest = std::min(lowest, mono);
            highest = std::max(highest, mono);
        }
        columns[column] = WaveformColumn{lowest, highest};
    }
    return columns;
}

} // namespace pixie::audio
