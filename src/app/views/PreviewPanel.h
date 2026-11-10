#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>

#include <cstdint>
#include <string>
#include <vector>

#include "app/AppContext.h"
#include "app/services/AudioPlayer.h"
#include "app/views/ConverterView.h"
#include "core/audio/AudioClip.h"
#include "core/formats/Aseprite.h"

namespace pixie::views {

// Side panel next to the library grid: shows one asset (image, animation or waveform), its metadata and a rename box.
class PreviewPanel {
public:
    PreviewPanel(app::AppContext& context, ConverterView& converter) : context_(context), converter_(converter) {}
    ~PreviewPanel() { Close(); }
    PreviewPanel(const PreviewPanel&) = delete;
    PreviewPanel& operator=(const PreviewPanel&) = delete;

    void Open(const library::Asset& asset);
    void OpenForTrim(const library::Asset& asset); // opens an audio asset with trim mode on
    void Close(); // also frees textures, so call it before the renderer is destroyed
    [[nodiscard]] bool IsOpen() const { return open_; }
    [[nodiscard]] const library::Asset& CurrentAsset() const { return asset_; }

    void Render(); // draws inside a child window the caller has already begun

private:
    // Size and date of a file on disk, read when the panel opens.
    struct FileInfo {
        std::uintmax_t bytes = 0;
        std::string modified;
    };

    void LoadImageAsset();
    void LoadAudioAsset();

    void RenderImageAsset();
    void RenderImagePanes();
    void RenderAsepriteMetadata();
    void AdvanceAnimation();

    void RenderAudioAsset();
    void RenderWaveform();
    void HandleWaveformMouse(ImVec2 origin, ImVec2 size);
    void RenderTrimControls();
    void BeginTrim();
    void EndTrim();
    void ApplyTrim();
    void RenderLoopControls();
    void BeginLoopMaker();
    void EndLoopMaker();
    void RefreshLoopPreview();
    void ApplyLoop();
    void RenderAudioMetadata();

    void RenderNameEditor(const char* hint);
    void RenderProjectList();

    bool RenameAsset(const std::string& requestedName, std::string& error); // renames the files on disk
    void ShowToolResult(const std::string& error);                           // reports a failed Open in X

    // Where the waveform is on screen, and conversions between sample frames and screen x.
    struct WaveformArea {
        ImVec2 origin;
        ImVec2 size;
        std::uint64_t frameCount;

        [[nodiscard]] float ToX(std::uint64_t frame) const {
            return origin.x + size.x * static_cast<float>(frame) / static_cast<float>(std::max<std::uint64_t>(1, frameCount));
        }
        [[nodiscard]] std::uint64_t ToFrame(float x) const {
            const float fraction = std::clamp((x - origin.x) / size.x, 0.0f, 1.0f);
            return static_cast<std::uint64_t>(fraction * static_cast<float>(frameCount));
        }
    };
    enum class TrimHandle { None, Start, End };
    void DrawTrimHandles(const WaveformArea& area) const;

    app::AppContext& context_;
    ConverterView& converter_;

    bool open_ = false;
    library::Asset asset_;
    char nameBuffer_[128] = "";
    std::string message_;

    // Image assets: the base image and the (possibly animated) .aseprite.
    SDL_Texture* baseTexture_ = nullptr;
    int baseWidth_ = 0;
    int baseHeight_ = 0;
    FileInfo baseFile_;
    std::vector<SDL_Texture*> frameTextures_;
    formats::AsepriteAnimation animation_; // durations and tags only; the pixels live in frameTextures_
    int asepriteWidth_ = 0;
    int asepriteHeight_ = 0;
    FileInfo asepriteFile_;
    int playingTag_ = -1; // index into animation_.tags; -1 = all frames
    int currentFrame_ = 0;
    Uint64 frameShownAtMs_ = 0;
    bool animationPlaying_ = true;

    // Audio assets.
    audio::AudioClip clip_;
    std::vector<audio::WaveformColumn> waveform_;
    std::string audioError_;
    FileInfo audioFile_;
    services::AudioPlayer player_;

    // Trim mode: keep [trimStart_, trimEnd_) of the clip, in sample frames.
    bool trimming_ = false;
    std::uint64_t trimStart_ = 0;
    std::uint64_t trimEnd_ = 0;
    TrimHandle draggedHandle_ = TrimHandle::None;

    // Make Loopable mode: clip_ holds the crossfaded preview and originalClip_ the untouched audio.
    bool loopMaking_ = false;
    audio::AudioClip originalClip_;
    int crossfadeMs_ = 200;
};

} // namespace pixie::views
