#include "app/services/AudioPlayer.h"

#include <algorithm>

namespace pixie::services {

// The rest of the clip is queued on the SDL stream at once, so position = total frames - frames still queued.

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
}

std::uint64_t AudioPlayer::Position() const {
    if (!stream_ || !clip_) return resumeFrame_;
    const int queuedBytes = SDL_GetAudioStreamQueued(stream_);
    if (queuedBytes <= 0) return resumeFrame_;
    const std::uint64_t bytesPerFrame = sizeof(float) * static_cast<std::uint64_t>(clip_->channels);
    const std::uint64_t framesLeft = static_cast<std::uint64_t>(queuedBytes) / bytesPerFrame;
    return clip_->FrameCount() > framesLeft ? clip_->FrameCount() - framesLeft : 0;
}

void AudioPlayer::Seek(std::uint64_t frame) {
    if (!stream_ || !clip_) return;
    const std::uint64_t frameCount = clip_->FrameCount();
    frame = std::min(frame, frameCount > 0 ? frameCount - 1 : 0);
    const size_t channels = static_cast<size_t>(clip_->channels);
    SDL_ClearAudioStream(stream_);
    SDL_PutAudioStreamData(stream_, clip_->samples.data() + frame * channels,
                           static_cast<int>((frameCount - frame) * channels * sizeof(float)));
    resumeFrame_ = frame;
}

void AudioPlayer::TogglePlay() {
    if (!stream_ || !clip_) return;
    if (playing_) {
        SDL_PauseAudioStreamDevice(stream_);
        playing_ = false;
        return;
    }
    if (SDL_GetAudioStreamQueued(stream_) <= 0) {
        Seek(resumeFrame_ >= clip_->FrameCount() ? 0 : resumeFrame_);
    }
    SDL_ResumeAudioStreamDevice(stream_);
    playing_ = true;
}

void AudioPlayer::Stop() {
    if (!stream_) return;
    SDL_ClearAudioStream(stream_);
    SDL_PauseAudioStreamDevice(stream_);
    playing_ = false;
    resumeFrame_ = 0;
}

void AudioPlayer::Update() {
    const bool reachedEnd = playing_ && stream_ && SDL_GetAudioStreamQueued(stream_) <= 0;
    if (!reachedEnd) return;
    SDL_PauseAudioStreamDevice(stream_);
    playing_ = false;
    resumeFrame_ = 0;
}

} // namespace pixie::services
