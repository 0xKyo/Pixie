#include "core/audio/OggWrite.h"

#include <vorbis/vorbisenc.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

#include "core/formats/BinaryFile.h"

namespace pixie::audio {

namespace {

constexpr int kFramesPerBlock = 4096;

void AppendPage(std::vector<unsigned char>& file, const ogg_page& page) {
    file.insert(file.end(), page.header, page.header + page.header_len);
    file.insert(file.end(), page.body, page.body + page.body_len);
}

// Pulls every finished packet out of the encoder and appends the resulting Ogg pages.
void FlushEncoder(vorbis_dsp_state& dsp, vorbis_block& block, ogg_stream_state& stream, std::vector<unsigned char>& file) {
    while (vorbis_analysis_blockout(&dsp, &block) == 1) {
        vorbis_analysis(&block, nullptr);
        vorbis_bitrate_addblock(&block);
        ogg_packet packet;
        while (vorbis_bitrate_flushpacket(&dsp, &packet)) {
            ogg_stream_packetin(&stream, &packet);
            ogg_page page;
            while (ogg_stream_pageout(&stream, &page)) AppendPage(file, page);
        }
    }
}

} // namespace

bool HasOggFormat(const std::filesystem::path& path, int channels, int sampleRate) {
    std::ifstream file(path, std::ios::binary);
    unsigned char head[256] = {};
    file.read(reinterpret_cast<char*>(head), sizeof(head));
    const size_t count = static_cast<size_t>(file.gcount());
    if (count < 4 || std::memcmp(head, "OggS", 4) != 0) return false;

    // The Vorbis identification packet: 0x01 "vorbis", version (4), channels (1), sample rate (4, little endian).
    static const unsigned char kMarker[7] = {0x01, 'v', 'o', 'r', 'b', 'i', 's'};
    for (size_t offset = 0; offset + 7 + 4 + 1 + 4 <= count; ++offset) {
        if (std::memcmp(head + offset, kMarker, sizeof(kMarker)) != 0) continue;
        const unsigned char* fields = head + offset + 7 + 4;
        const int fileChannels = fields[0];
        const int fileRate = fields[1] | (fields[2] << 8) | (fields[3] << 16) | (fields[4] << 24);
        return fileChannels == channels && fileRate == sampleRate;
    }
    return false;
}

bool SaveOgg(const AudioClip& clip, const std::filesystem::path& path, float quality, std::string* error) {
    if (clip.Empty()) {
        if (error) *error = "the clip has no samples";
        return false;
    }

    vorbis_info info;
    vorbis_info_init(&info);
    if (vorbis_encode_init_vbr(&info, clip.channels, clip.sampleRate, std::clamp(quality, -0.1f, 1.0f)) != 0) {
        vorbis_info_clear(&info);
        if (error) *error = "the Vorbis encoder does not support " + std::to_string(clip.channels) + " channels at " +
                            std::to_string(clip.sampleRate) + " Hz";
        return false;
    }

    vorbis_comment comment;
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, "ENCODER", "Pixie");

    vorbis_dsp_state dsp;
    vorbis_block block;
    vorbis_analysis_init(&dsp, &info);
    vorbis_block_init(&dsp, &block);

    ogg_stream_state stream;
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    ogg_stream_init(&stream, std::rand());

    std::vector<unsigned char> file;
    ogg_packet header, headerComments, headerCodebooks;
    vorbis_analysis_headerout(&dsp, &comment, &header, &headerComments, &headerCodebooks);
    ogg_stream_packetin(&stream, &header);
    ogg_stream_packetin(&stream, &headerComments);
    ogg_stream_packetin(&stream, &headerCodebooks);
    ogg_page page;
    while (ogg_stream_flush(&stream, &page)) AppendPage(file, page); // headers go in their own pages

    const uint64_t totalFrames = clip.FrameCount();
    for (uint64_t start = 0; start < totalFrames; start += kFramesPerBlock) {
        const int blockFrames = static_cast<int>(std::min<uint64_t>(kFramesPerBlock, totalFrames - start));
        float** channelBuffers = vorbis_analysis_buffer(&dsp, blockFrames);
        for (int frame = 0; frame < blockFrames; ++frame) {
            for (int channel = 0; channel < clip.channels; ++channel) {
                channelBuffers[channel][frame] = clip.samples[(start + frame) * clip.channels + channel];
            }
        }
        vorbis_analysis_wrote(&dsp, blockFrames);
        FlushEncoder(dsp, block, stream, file);
    }
    vorbis_analysis_wrote(&dsp, 0); // end of stream
    FlushEncoder(dsp, block, stream, file);

    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);

    return formats::WriteWholeFile(path, file.data(), file.size(), error);
}

} // namespace pixie::audio
