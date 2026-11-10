#include "app/services/AudioPlayer.h"

#include <algorithm>

namespace pixie::services {

// The rest of the range is queued on the SDL stream at once, so position = range end - frames still queued.
// When looping, one extra pass is queued behind it, so only the frames of the current pass count.

std::string AudioPlayer::Load(const audio::AudioClip* clip) {
    Unload();
    clip_ = clip;
    if (!clip_ || clip_->Empty()) return {};
    const SDL_AudioSpec format{SDL_AUDIO_F32, clip_->channels, clip_->sampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
    if (!stream_) return std::string("No audio output available: ") + SDL_GetError();
    return {};
}

void AudioPlayer::Unload() {
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
    clip_ = nullptr;
    playing_ = false;
    resumeFrame_ = 0;
    rangeFirst_ = 0;
    rangeEnd_ = 0;
}

bool AudioPlayer::QueueIsDrained() const {
    if (!stream_ || !clip_) return true;
    const int bytesPerFrame = static_cast<int>(sizeof(float)) * clip_->channels;
    const int fiveMillisecondsOfBytes = bytesPerFrame * clip_->sampleRate / 200;
    const int threshold = std::min(fiveMillisecondsOfBytes, queuedAtSeekBytes_ / 4); // short ranges get a smaller threshold
    return SDL_GetAudioStreamQueued(stream_) <= threshold;
}

std::uint64_t AudioPlayer::Position() const {
    if (!stream_ || !clip_ || QueueIsDrained()) return resumeFrame_;
    const int queuedBytes = SDL_GetAudioStreamQueued(stream_);
    const std::uint64_t bytesPerFrame = sizeof(float) * static_cast<std::uint64_t>(clip_->channels);
    std::uint64_t framesLeft = static_cast<std::uint64_t>(queuedBytes) / bytesPerFrame;
    const std::uint64_t rangeLength = RangeEnd() - RangeFirst();
    if (looping_ && rangeLength > 0 && framesLeft > 0) framesLeft = (framesLeft - 1) % rangeLength + 1;
    return RangeEnd() > framesLeft ? std::max(RangeFirst(), RangeEnd() - framesLeft) : RangeFirst();
}

void AudioPlayer::Seek(std::uint64_t frame) {
    if (!stream_ || !clip_) return;
    const std::uint64_t end = RangeEnd();
    if (end <= RangeFirst()) return;
    frame = std::clamp(frame, RangeFirst(), end - 1);
    const size_t channels = static_cast<size_t>(clip_->channels);
    SDL_ClearAudioStream(stream_);
    queuedAtSeekBytes_ = static_cast<int>((end - frame) * channels * sizeof(float));
    SDL_PutAudioStreamData(stream_, clip_->samples.data() + frame * channels, queuedAtSeekBytes_);
    resumeFrame_ = frame;
}

void AudioPlayer::TogglePlay() {
    if (!stream_ || !clip_) return;
    if (playing_) {
        SDL_PauseAudioStreamDevice(stream_);
        playing_ = false;
        return;
    }
    if (QueueIsDrained()) {
        const bool resumeIsInRange = resumeFrame_ >= RangeFirst() && resumeFrame_ < RangeEnd();
        Seek(resumeIsInRange ? resumeFrame_ : RangeFirst());
    }
    SDL_ResumeAudioStreamDevice(stream_);
    playing_ = true;
}

void AudioPlayer::Stop() {
    if (!stream_) return;
    SDL_ClearAudioStream(stream_);
    SDL_PauseAudioStreamDevice(stream_);
    playing_ = false;
    resumeFrame_ = RangeFirst();
}

void AudioPlayer::Update() {
    if (looping_ && playing_ && stream_ && clip_) {
        QueueNextPassIfNeeded();
        return;
    }
    const bool reachedEnd = playing_ && stream_ && QueueIsDrained();
    if (!reachedEnd) return;
    SDL_ClearAudioStream(stream_); // drop the few samples SDL keeps
    SDL_PauseAudioStreamDevice(stream_);
    playing_ = false;
    resumeFrame_ = RangeFirst();
}

void AudioPlayer::QueueNextPassIfNeeded() {
    const std::uint64_t first = RangeFirst();
    const std::uint64_t end = RangeEnd();
    if (end <= first) return;
    const size_t channels = static_cast<size_t>(clip_->channels);
    const int passBytes = static_cast<int>((end - first) * channels * sizeof(float));
    if (SDL_GetAudioStreamQueued(stream_) > passBytes) return; // a whole extra pass is already waiting
    SDL_PutAudioStreamData(stream_, clip_->samples.data() + first * channels, passBytes);
}

void AudioPlayer::SetLooping(bool looping) {
    if (looping == looping_) return;
    const std::uint64_t position = Position(); // measured with the old queue layout
    looping_ = looping;
    if (!stream_ || !clip_) return;
    Seek(position); // re-queues with the new layout
}

void AudioPlayer::SetRange(std::uint64_t firstFrame, std::uint64_t endFrame) {
    if (!clip_) return;
    const std::uint64_t position = Position();
    rangeFirst_ = firstFrame;
    rangeEnd_ = endFrame;
    const bool positionIsInRange = position >= rangeFirst_ && position < endFrame;
    Seek(positionIsInRange ? position : rangeFirst_); // re-queues, so a playing clip keeps playing from here
}

void AudioPlayer::ClearRange() {
    if (!clip_) return;
    const std::uint64_t position = Position();
    rangeFirst_ = 0;
    rangeEnd_ = 0;
    Seek(position);
}

} // namespace pixie::services
