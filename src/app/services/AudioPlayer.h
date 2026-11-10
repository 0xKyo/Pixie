#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "core/audio/AudioClip.h"

namespace pixie::services {

// Plays one decoded clip (or just a range of it) on the default output device; positions are in sample frames.
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
    void Stop(); // pauses and rewinds to the start of the range
    void Seek(std::uint64_t frame);
    void Update(); // call every frame to notice when playback reaches the end

    // Limits playback to [firstFrame, endFrame); the playhead jumps into the range if it was outside.
    void SetRange(std::uint64_t firstFrame, std::uint64_t endFrame);
    void ClearRange();

    // When on, playback restarts at the start of the range instead of stopping; the choice survives Load.
    void SetLooping(bool looping);
    [[nodiscard]] bool IsLooping() const { return looping_; }

private:
    // SDL's resampler keeps a few samples queued forever, so "finished" means "almost nothing left".
    [[nodiscard]] bool QueueIsDrained() const;
    void QueueNextPassIfNeeded();
    [[nodiscard]] std::uint64_t RangeFirst() const { return rangeFirst_; }
    [[nodiscard]] std::uint64_t RangeEnd() const { return rangeEnd_ > 0 ? rangeEnd_ : clip_->FrameCount(); }

    const audio::AudioClip* clip_ = nullptr;
    SDL_AudioStream* stream_ = nullptr;
    bool playing_ = false;
    bool looping_ = false;
    int queuedAtSeekBytes_ = 0;     // how much the last Seek queued, to scale the "drained" threshold
    std::uint64_t resumeFrame_ = 0; // where Play starts when nothing is queued
    std::uint64_t rangeFirst_ = 0;
    std::uint64_t rangeEnd_ = 0;    // 0 = up to the end of the clip
};

} // namespace pixie::services
