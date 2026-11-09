#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "core/audio/AudioClip.h"

namespace pixie::services {

// Plays one decoded clip on the default output device; positions are in sample frames.
class AudioPlayer {
public:
    AudioPlayer() = default;
    ~AudioPlayer() { Unload(); }
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Starts using `clip` (must outlive Unload); returns an error if there's no audio device.
    std::string Load(const audio::AudioClip* clip);
    void Unload();

    [[nodiscard]] bool HasDevice() const { return stream_ != nullptr; }
    [[nodiscard]] bool IsPlaying() const { return playing_; }
    [[nodiscard]] std::uint64_t Position() const;

    void TogglePlay();
    void Stop(); // pauses and rewinds to the start
    void Seek(std::uint64_t frame);
    void Update(); // call every frame to notice when playback reaches the end

private:
    const audio::AudioClip* clip_ = nullptr;
    SDL_AudioStream* stream_ = nullptr;
    bool playing_ = false;
    std::uint64_t resumeFrame_ = 0; // where Play starts when nothing is queued
};

} // namespace pixie::services
