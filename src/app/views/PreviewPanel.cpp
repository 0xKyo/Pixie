#include "app/views/PreviewPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "app/services/ExternalTools.h"
#include "app/util/Dialogs.h"
#include "app/util/Gui.h"
#include "app/util/Text.h"
#include "app/util/Texture.h"
#include "core/audio/LoopMaker.h"
#include "core/audio/SilenceTrim.h"
#include "core/audio/WavWrite.h"
#include "core/formats/BinaryFile.h"
#include "core/formats/ImageFile.h"

namespace pixie::views {

namespace {

constexpr float kImagePaneHeight = 240.0f;
constexpr float kWaveformHeight = 120.0f;
constexpr int kWaveformColumns = 1024;
constexpr float kHandleGrabRadius = 9.0f; // wider than the drawn handle so it is easy to grab
constexpr std::uint64_t kMaxPreviewAudioBytes = 400ull * 1024 * 1024; // decoded float samples

// A dark box filling `size`, with the texture centered and pixel-perfect inside it.
void DrawImageBox(SDL_Texture* texture, int width, int height, ImVec2 size) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(24, 24, 28, 255));
    if (texture) {
        const ImVec2 drawn = util::FitInsidePixelPerfect(width, height, size);
        const ImVec2 imageMin(origin.x + (size.x - drawn.x) * 0.5f, origin.y + (size.y - drawn.y) * 0.5f);
        drawList->AddImage(util::ToImGui(texture), imageMin, ImVec2(imageMin.x + drawn.x, imageMin.y + drawn.y));
    }
    ImGui::Dummy(size);
}

// "Label     value", with the values lined up in a column.
void MetadataRow(const char* label, const std::string& value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(110.0f);
    ImGui::TextWrapped("%s", value.c_str());
}

std::string Pixels(int width, int height) { return std::to_string(width) + " x " + std::to_string(height) + " px"; }

// Validates a new file name; returns the trimmed name, or "" with `error` set.
std::string CleanFileName(std::string name, std::string& error) {
    while (!name.empty() && name.back() == ' ') name.pop_back();
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    if (name.empty()) {
        error = "The name can't be empty";
        return {};
    }
    if (name.find_first_of("<>:\"/\\|?*") != std::string::npos || name.back() == '.') {
        error = "The name can't contain < > : \" / \\ | ? * or end with a dot";
        return {};
    }
    return name;
}

} // namespace

// ---- Opening and closing ----

void PreviewPanel::Open(const library::Asset& asset) {
    Close();
    asset_ = asset;
    message_.clear();
    std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", asset.name.c_str());
    if (asset.IsAudio()) {
        LoadAudioAsset();
    } else {
        LoadImageAsset();
    }
    open_ = true;
}

void PreviewPanel::Close() {
    open_ = false;
    trimming_ = false;
    loopMaking_ = false;
    originalClip_ = audio::AudioClip{};
    draggedHandle_ = TrimHandle::None;
    player_.Unload(); // before clip_ is cleared: the player reads its samples
    clip_ = audio::AudioClip{};
    waveform_.clear();
    audioError_.clear();
    util::DestroyTexture(baseTexture_);
    for (SDL_Texture*& texture : frameTextures_) util::DestroyTexture(texture);
    frameTextures_.clear();
    animation_ = formats::AsepriteAnimation{};
}

void PreviewPanel::LoadImageAsset() {
    playingTag_ = -1;
    currentFrame_ = 0;
    frameShownAtMs_ = SDL_GetTicks();
    baseWidth_ = baseHeight_ = asepriteWidth_ = asepriteHeight_ = 0;
    baseFile_ = FileInfo{};
    asepriteFile_ = FileInfo{};

    if (asset_.HasBaseImage()) {
        graphics::Image base;
        std::string error;
        if (formats::LoadImageFile(asset_.baseImagePath, base, &error)) {
            baseTexture_ = util::CreateTexture(context_.renderer, base);
            baseWidth_ = base.Width();
            baseHeight_ = base.Height();
        } else {
            message_ = "Could not read base image: " + error;
        }
        baseFile_ = FileInfo{util::FileSize(asset_.baseImagePath), util::FormatModified(asset_.baseImagePath)};
    }

    if (asset_.HasAseprite()) {
        std::vector<unsigned char> bytes;
        std::string error;
        if (formats::ReadWholeFile(asset_.asepritePath, bytes, &error) &&
            formats::DecodeAseprite(bytes.data(), bytes.size(), animation_, &error)) {
            for (const graphics::Image& frame : animation_.frames) {
                frameTextures_.push_back(util::CreateTexture(context_.renderer, frame));
            }
            if (!animation_.frames.empty()) {
                asepriteWidth_ = animation_.frames.front().Width();
                asepriteHeight_ = animation_.frames.front().Height();
            }
            animation_.frames.clear(); // the textures hold the pixels now
        } else {
            message_ = "Could not read .aseprite: " + error;
        }
        asepriteFile_ = FileInfo{util::FileSize(asset_.asepritePath), util::FormatModified(asset_.asepritePath)};
    }
}

void PreviewPanel::LoadAudioAsset() {
    audioFile_ = FileInfo{util::FileSize(asset_.audioPath), util::FormatModified(asset_.audioPath)};

    std::string error;
    if (!audio::LoadClip(asset_.audioPath, clip_, &error)) {
        clip_ = audio::AudioClip{};
        audioError_ = error;
        return;
    }
    if (clip_.samples.size() * sizeof(float) > kMaxPreviewAudioBytes) {
        clip_ = audio::AudioClip{};
        audioError_ = "Too long to preview in memory";
        return;
    }
    waveform_ = audio::ComputeWaveform(clip_, kWaveformColumns);
    audioError_ = player_.Load(&clip_);
}

// ---- Image assets ----

void PreviewPanel::Render() {
    if (asset_.IsAudio()) {
        RenderAudioAsset();
    } else {
        RenderImageAsset();
    }
}

void PreviewPanel::RenderImageAsset() {
    if (ImGui::Button("Close")) {
        Close();
        return;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!asset_.HasBaseImage());
    if (ImGui::Button("Convert...")) {
        const library::Asset asset = asset_; // Close() resets asset_
        Close();
        converter_.OpenAsset(asset);
        ImGui::EndDisabled();
        return;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!asset_.HasAseprite());
    if (ImGui::Button("Open in Aseprite")) ShowToolResult(services::OpenInAseprite(asset_.asepritePath));
    ImGui::EndDisabled();
    if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
    ImGui::Separator();

    AdvanceAnimation();
    RenderImagePanes();
    ImGui::Separator();

    RenderNameEditor("Renames the base and the .aseprite together");
    ImGui::Separator();

    ImGui::TextUnformatted("Metadata");
    MetadataRow("Folder", asset_.folder.empty() ? "(library root)" : asset_.folder);
    ImGui::Spacing();
    if (asset_.HasBaseImage()) {
        ImGui::TextUnformatted(("Base " + asset_.baseImageFormat).c_str());
        MetadataRow("File", asset_.baseImagePath.filename().string());
        MetadataRow("Size", Pixels(baseWidth_, baseHeight_));
        MetadataRow("File size", util::FormatBytes(baseFile_.bytes));
        MetadataRow("Modified", baseFile_.modified);
    } else {
        ImGui::TextDisabled("No PNG/JPG base");
    }
    ImGui::Spacing();
    if (asset_.HasAseprite()) {
        RenderAsepriteMetadata();
    } else {
        ImGui::TextDisabled("Not converted yet");
    }
    ImGui::Separator();

    RenderProjectList();
}

// Base and .aseprite side by side (or one pane filling the width if only one exists).
void PreviewPanel::RenderImagePanes() {
    const bool showBoth = asset_.HasBaseImage() && asset_.HasAseprite();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 paneSize(showBoth ? (width - 8.0f) * 0.5f : width, kImagePaneHeight - ImGui::GetTextLineHeightWithSpacing());

    if (asset_.HasBaseImage()) {
        ImGui::BeginGroup();
        ImGui::Text("Base (%dx%d)", baseWidth_, baseHeight_);
        DrawImageBox(baseTexture_, baseWidth_, baseHeight_, paneSize);
        ImGui::EndGroup();
    }
    if (showBoth) ImGui::SameLine();
    if (asset_.HasAseprite()) {
        const int frameCount = static_cast<int>(frameTextures_.size());
        ImGui::BeginGroup();
        if (frameCount > 1) {
            ImGui::Text("Aseprite (%dx%d) - frame %d/%d", asepriteWidth_, asepriteHeight_, currentFrame_ + 1, frameCount);
        } else {
            ImGui::Text("Aseprite (%dx%d)", asepriteWidth_, asepriteHeight_);
        }
        SDL_Texture* frame = currentFrame_ < frameCount ? frameTextures_[currentFrame_] : nullptr;
        DrawImageBox(frame, asepriteWidth_, asepriteHeight_, paneSize);
        ImGui::EndGroup();
    }
}

// Steps to the next frame once the current one has been shown for its duration (looping inside the tag).
void PreviewPanel::AdvanceAnimation() {
    const int frameCount = static_cast<int>(frameTextures_.size());
    if (frameCount <= 1) {
        currentFrame_ = 0;
        return;
    }

    int firstFrame = 0;
    int lastFrame = frameCount - 1;
    if (playingTag_ >= 0 && playingTag_ < static_cast<int>(animation_.tags.size())) {
        firstFrame = std::clamp(animation_.tags[playingTag_].firstFrame, 0, lastFrame);
        lastFrame = std::clamp(animation_.tags[playingTag_].lastFrame, firstFrame, lastFrame);
    }
    if (currentFrame_ < firstFrame || currentFrame_ > lastFrame) {
        currentFrame_ = firstFrame;
        frameShownAtMs_ = SDL_GetTicks();
    }
    if (!animationPlaying_) return;

    const Uint64 now = SDL_GetTicks();
    const int durationMs = std::max(1, animation_.frameDurationsMs[currentFrame_]);
    if (now - frameShownAtMs_ >= static_cast<Uint64>(durationMs)) {
        currentFrame_ = currentFrame_ >= lastFrame ? firstFrame : currentFrame_ + 1;
        frameShownAtMs_ = now;
    }
}

void PreviewPanel::RenderAsepriteMetadata() {
    const int frameCount = static_cast<int>(frameTextures_.size());
    ImGui::TextUnformatted("Aseprite");
    MetadataRow("File", asset_.asepritePath.filename().string());
    MetadataRow("Canvas", Pixels(asepriteWidth_, asepriteHeight_));
    MetadataRow("Frames", std::to_string(frameCount));
    if (frameCount > 1) {
        int totalMs = 0;
        for (const int durationMs : animation_.frameDurationsMs) totalMs += durationMs;
        MetadataRow("Duration", std::to_string(totalMs) + " ms");
    }
    MetadataRow("File size", util::FormatBytes(asepriteFile_.bytes));
    MetadataRow("Modified", asepriteFile_.modified);
    if (frameCount <= 1) return;

    ImGui::Spacing();
    ImGui::TextUnformatted("Animation");
    if (ImGui::Button(animationPlaying_ ? "Pause" : "Play")) {
        animationPlaying_ = !animationPlaying_;
        frameShownAtMs_ = SDL_GetTicks();
    }
    if (ImGui::Selectable("All frames", playingTag_ < 0)) playingTag_ = -1;
    for (int tagIndex = 0; tagIndex < static_cast<int>(animation_.tags.size()); ++tagIndex) {
        const auto& tag = animation_.tags[tagIndex];
        const std::string label = tag.name + "  (frames " + std::to_string(tag.firstFrame + 1) + "-" + std::to_string(tag.lastFrame + 1) + ")";
        ImGui::PushID(tagIndex);
        if (ImGui::Selectable(label.c_str(), playingTag_ == tagIndex)) playingTag_ = tagIndex;
        ImGui::PopID();
    }
    if (animation_.tags.empty()) ImGui::TextDisabled("No tags (exports as \"default\")");
}

// ---- Audio assets ----

void PreviewPanel::RenderAudioAsset() {
    if (ImGui::Button("Close")) {
        Close();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Open in Audacity")) ShowToolResult(services::OpenInAudacity(asset_.audioPath, context_.settings));
    if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
    ImGui::Separator();

    if (!audioError_.empty()) {
        ImGui::TextWrapped("%s", audioError_.c_str());
        ImGui::TextDisabled("You can still open it in Audacity.");
    }
    if (!clip_.Empty()) RenderWaveform();
    ImGui::Separator();

    RenderNameEditor(nullptr);
    ImGui::Separator();

    RenderAudioMetadata();
    ImGui::Separator();

    RenderProjectList();
}

// The waveform with the playhead (click or drag to seek); in trim mode also the two cut handles.
void PreviewPanel::RenderWaveform() {
    player_.Update();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, kWaveformHeight);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##waveform", size);
    HandleWaveformMouse(origin, size);

    const WaveformArea area{origin, size, clip_.FrameCount()};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(24, 24, 28, 255));

    const float centerY = origin.y + size.y * 0.5f;
    const int pixelColumns = static_cast<int>(size.x);
    for (int x = 0; x < pixelColumns && !waveform_.empty(); ++x) {
        const audio::WaveformColumn& column = waveform_[static_cast<size_t>(x) * waveform_.size() / std::max(1, pixelColumns)];
        const float top = centerY - std::clamp(column.max, -1.0f, 1.0f) * size.y * 0.5f;
        const float bottom = centerY - std::clamp(column.min, -1.0f, 1.0f) * size.y * 0.5f;
        drawList->AddLine(ImVec2(origin.x + x + 0.5f, top), ImVec2(origin.x + x + 0.5f, std::max(bottom, top + 1.0f)),
                          IM_COL32(110, 170, 255, 255));
    }
    if (trimming_) DrawTrimHandles(area);

    const float playheadX = area.ToX(player_.Position());
    drawList->AddLine(ImVec2(playheadX, origin.y), ImVec2(playheadX, origin.y + size.y), IM_COL32(255, 200, 80, 255), 2.0f);

    ImGui::BeginDisabled(!player_.HasDevice());
    if (ImGui::Button(player_.IsPlaying() ? "Pause" : "Play")) player_.TogglePlay();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) player_.Stop();
    ImGui::SameLine();
    bool looping = player_.IsLooping();
    if (ImGui::Checkbox("Loop", &looping)) player_.SetLooping(looping);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%s / %s", util::FormatDuration(static_cast<double>(player_.Position()) / clip_.sampleRate).c_str(),
                util::FormatDuration(clip_.DurationSeconds()).c_str());
    RenderTrimControls();
    if (!trimming_) RenderLoopControls();
}

// A mouse press on a cut handle drags it; anywhere else it seeks. The player's range updates when the drag ends.
void PreviewPanel::HandleWaveformMouse(ImVec2 origin, ImVec2 size) {
    const WaveformArea area{origin, size, clip_.FrameCount()};
    const float mouseX = ImGui::GetMousePos().x;
    const std::uint64_t minimumKeptFrames = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(clip_.sampleRate) / 100); // 10 ms

    if (trimming_) {
        const float distanceToStart = std::fabs(mouseX - area.ToX(trimStart_));
        const float distanceToEnd = std::fabs(mouseX - area.ToX(trimEnd_));
        const bool nearStart = distanceToStart <= kHandleGrabRadius && distanceToStart <= distanceToEnd;
        const bool nearEnd = distanceToEnd <= kHandleGrabRadius && !nearStart;
        if (ImGui::IsItemHovered() && (nearStart || nearEnd)) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActivated()) draggedHandle_ = nearStart ? TrimHandle::Start : nearEnd ? TrimHandle::End : TrimHandle::None;
    }

    if (ImGui::IsItemActive()) {
        const std::uint64_t frameUnderMouse = area.ToFrame(mouseX);
        if (draggedHandle_ == TrimHandle::Start) {
            trimStart_ = std::min(frameUnderMouse, trimEnd_ - minimumKeptFrames);
        } else if (draggedHandle_ == TrimHandle::End) {
            trimEnd_ = std::max(frameUnderMouse, trimStart_ + minimumKeptFrames);
        } else if (player_.HasDevice()) {
            player_.Seek(frameUnderMouse);
        }
    }
    if (ImGui::IsItemDeactivated() && draggedHandle_ != TrimHandle::None) {
        player_.SetRange(trimStart_, trimEnd_);
        draggedHandle_ = TrimHandle::None;
    }
}

// Darkens the part that will be cut and draws a grip on each cut point.
void PreviewPanel::DrawTrimHandles(const WaveformArea& area) const {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float left = area.origin.x;
    const float right = area.origin.x + area.size.x;
    const float top = area.origin.y;
    const float bottom = area.origin.y + area.size.y;
    const float startX = area.ToX(trimStart_);
    const float endX = area.ToX(trimEnd_);

    const ImU32 cutColor = IM_COL32(0, 0, 0, 170);
    drawList->AddRectFilled(ImVec2(left, top), ImVec2(startX, bottom), cutColor);
    drawList->AddRectFilled(ImVec2(endX, top), ImVec2(right, bottom), cutColor);

    for (const float x : {startX, endX}) {
        drawList->AddLine(ImVec2(x, top), ImVec2(x, bottom), IM_COL32(255, 140, 60, 255), 2.0f);
        const float centerY = (top + bottom) * 0.5f;
        drawList->AddRectFilled(ImVec2(x - 4.0f, centerY - 12.0f), ImVec2(x + 4.0f, centerY + 12.0f), IM_COL32(255, 140, 60, 255), 2.0f);
    }
}

// ---- Trim Silence ----

void PreviewPanel::OpenForTrim(const library::Asset& asset) {
    Open(asset);
    if (asset.IsAudio()) BeginTrim();
}

// Starts trim mode with the cut points on the first and last audible sample.
void PreviewPanel::BeginTrim() {
    EndLoopMaker(); // the two modes don't mix
    if (clip_.Empty()) {
        message_ = "This audio can't be previewed, so it can't be trimmed here";
        return;
    }
    if (!audio::CanTrim(asset_.audioPath)) {
        message_ = "Only WAV files can be trimmed";
        return;
    }
    audio::FrameRange sound;
    if (!audio::FindSoundRange(clip_, audio::SilenceSettings{}, sound)) {
        message_ = "This file is completely silent";
        return;
    }
    trimStart_ = sound.firstFrame;
    trimEnd_ = sound.endFrame;
    trimming_ = true;
    draggedHandle_ = TrimHandle::None;
    player_.SetRange(trimStart_, trimEnd_);
    const bool nothingToCut = trimStart_ == 0 && trimEnd_ == clip_.FrameCount();
    message_ = nothingToCut ? "No silence found; drag the handles to cut anyway" : "";
}

void PreviewPanel::EndTrim() {
    trimming_ = false;
    draggedHandle_ = TrimHandle::None;
    player_.ClearRange();
}

void PreviewPanel::RenderTrimControls() {
    if (!trimming_) {
        const bool canTrim = !clip_.Empty() && audio::CanTrim(asset_.audioPath);
        ImGui::BeginDisabled(!canTrim);
        if (ImGui::Button("Trim Silence")) BeginTrim();
        ImGui::EndDisabled();
        if (!canTrim && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Only WAV files can be trimmed");
        return;
    }

    const double rate = clip_.sampleRate;
    const double cutFromStart = static_cast<double>(trimStart_) / rate;
    const double cutFromEnd = static_cast<double>(clip_.FrameCount() - trimEnd_) / rate;
    ImGui::TextWrapped("Drag the orange handles to choose what to keep. Play previews only that part.");
    ImGui::Text("Keep %s - %s (%s)", util::FormatDuration(cutFromStart).c_str(), util::FormatDuration(static_cast<double>(trimEnd_) / rate).c_str(),
                util::FormatDuration(static_cast<double>(trimEnd_ - trimStart_) / rate).c_str());
    ImGui::TextDisabled("Cutting %s from the start and %s from the end", util::FormatDuration(cutFromStart).c_str(),
                        util::FormatDuration(cutFromEnd).c_str());

    if (ImGui::Button("Apply Trim")) ApplyTrim();
    ImGui::SameLine();
    if (ImGui::Button("Reset")) BeginTrim();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) EndTrim();
}

// Overwrites the WAV file with the kept part, after asking.
void PreviewPanel::ApplyTrim() {
    if (trimStart_ == 0 && trimEnd_ == clip_.FrameCount()) {
        message_ = "Nothing to cut";
        return;
    }
    const std::string name = asset_.audioPath.filename().string();
    const double rate = clip_.sampleRate;
    const std::string summary = util::FormatDuration(clip_.DurationSeconds()) + " -> " + util::FormatDuration(static_cast<double>(trimEnd_ - trimStart_) / rate);
    if (!util::Confirm(context_.window, "Trim " + name + "?\n\n" + summary + "\n\nThe file will be overwritten.", "Trim")) return;

    std::string error;
    if (!audio::TrimWavFile(asset_.audioPath, audio::FrameRange{trimStart_, trimEnd_}, &error)) {
        message_ = "Trim failed: " + error;
        return;
    }
    const library::Asset asset = asset_;
    Open(asset); // reload the shortened file
    message_ = "Trimmed " + name + ": " + summary;
    context_.statusMessage = message_;
}

// ---- Make Loopable ----

// Starts loop mode: the clip is replaced by a crossfaded copy that plays in a loop, so you can hear the seam.
void PreviewPanel::BeginLoopMaker() {
    if (clip_.Empty() || !audio::CanTrim(asset_.audioPath)) {
        message_ = "Only WAV files can be made loopable";
        return;
    }
    originalClip_ = clip_;
    loopMaking_ = true;
    message_.clear();
    RefreshLoopPreview();
    player_.SetLooping(true);
    player_.TogglePlay();
}

void PreviewPanel::EndLoopMaker() {
    if (!loopMaking_) return;
    loopMaking_ = false;
    player_.Unload(); // before clip_ changes: the player reads its samples
    clip_ = std::move(originalClip_);
    originalClip_ = audio::AudioClip{};
    waveform_ = audio::ComputeWaveform(clip_, kWaveformColumns);
    audioError_ = player_.Load(&clip_);
}

void PreviewPanel::RefreshLoopPreview() {
    const std::uint64_t requested = static_cast<std::uint64_t>(originalClip_.sampleRate) * static_cast<std::uint64_t>(crossfadeMs_) / 1000;
    player_.Unload();
    clip_ = audio::MakeLoopable(originalClip_, std::min(requested, audio::MaxLoopCrossfadeFrames(originalClip_)));
    waveform_ = audio::ComputeWaveform(clip_, kWaveformColumns);
    audioError_ = player_.Load(&clip_);
}

void PreviewPanel::RenderLoopControls() {
    if (!loopMaking_) {
        const bool canLoop = !clip_.Empty() && audio::CanTrim(asset_.audioPath);
        ImGui::BeginDisabled(!canLoop);
        if (ImGui::Button("Make Loopable")) BeginLoopMaker();
        ImGui::EndDisabled();
        if (!canLoop && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Only WAV files can be made loopable");
        return;
    }

    const int maxMs = std::max(10, std::min(2000, static_cast<int>(audio::MaxLoopCrossfadeFrames(originalClip_) * 1000 / std::max(1, originalClip_.sampleRate))));
    ImGui::TextWrapped("The end is blended into the start, so the audio gets shorter by the crossfade. Playing in a loop to preview the seam.");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##crossfade", &crossfadeMs_, 10, maxMs, "Crossfade %d ms");
    crossfadeMs_ = std::clamp(crossfadeMs_, 10, maxMs);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        RefreshLoopPreview();
        player_.SetLooping(true);
        player_.TogglePlay();
    }
    if (ImGui::Button("Apply Loop")) ApplyLoop();
    ImGui::SameLine();
    if (ImGui::Button("Cancel##loop")) EndLoopMaker();
}

// Overwrites the WAV with the crossfaded clip (16-bit PCM), after asking.
void PreviewPanel::ApplyLoop() {
    const std::string name = asset_.audioPath.filename().string();
    const std::string summary = util::FormatDuration(originalClip_.DurationSeconds()) + " -> " + util::FormatDuration(clip_.DurationSeconds());
    if (!util::Confirm(context_.window, "Make " + name + " loopable?\n\n" + summary + "\n\nThe file will be overwritten (saved as 16-bit WAV).", "Apply")) return;

    std::filesystem::path temporary = asset_.audioPath;
    temporary += ".looping";
    std::string error;
    std::error_code renameError;
    if (!audio::SaveWav(clip_, temporary, &error)) {
        message_ = "Make Loopable failed: " + error;
        return;
    }
    std::filesystem::rename(temporary, asset_.audioPath, renameError);
    if (renameError) {
        std::filesystem::remove(temporary, renameError);
        message_ = "Make Loopable failed: could not replace the file";
        return;
    }
    const library::Asset asset = asset_;
    Open(asset); // reload the new file
    message_ = "Made " + name + " loopable: " + summary;
    context_.statusMessage = message_;
}

void PreviewPanel::RenderAudioMetadata() {
    ImGui::TextUnformatted("Metadata");
    MetadataRow("Folder", asset_.folder.empty() ? "(library root)" : asset_.folder);
    MetadataRow("File", asset_.audioPath.filename().string());
    MetadataRow("Format", asset_.audioFormat);
    if (!clip_.Empty()) {
        MetadataRow("Codec", clip_.codec);
        MetadataRow("Duration", util::FormatDuration(clip_.DurationSeconds()) + " (" + std::to_string(clip_.FrameCount()) + " samples)");
        MetadataRow("Sample rate", std::to_string(clip_.sampleRate) + " Hz");
        const std::string channels = clip_.channels == 1   ? "1 (mono)"
                                     : clip_.channels == 2 ? "2 (stereo)"
                                                           : std::to_string(clip_.channels);
        MetadataRow("Channels", channels);
    }
    MetadataRow("File size", util::FormatBytes(audioFile_.bytes));
    MetadataRow("Modified", audioFile_.modified);
}

// ---- Shared sections ----

void PreviewPanel::RenderNameEditor(const char* hint) {
    ImGui::TextUnformatted("Name");
    ImGui::SetNextItemWidth(-1.0f);
    const bool pressedEnter = ImGui::InputText("##AssetName", nameBuffer_, sizeof(nameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);
    const bool edited = std::string(nameBuffer_) != asset_.name;

    ImGui::BeginDisabled(!edited);
    if (ImGui::Button("Rename") || (pressedEnter && edited)) {
        std::string error;
        if (RenameAsset(nameBuffer_, error)) {
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", asset_.name.c_str());
            message_ = "Renamed to " + asset_.name;
        } else {
            message_ = error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert")) std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", asset_.name.c_str());
    ImGui::EndDisabled();
    if (hint) ImGui::TextDisabled("%s", hint);
}

void PreviewPanel::RenderProjectList() {
    ImGui::TextUnformatted("Projects");
    bool inAnyProject = false;
    for (const services::Project& project : context_.projects.All()) {
        if (!project.assetKeys.count(asset_.key)) continue;
        ImGui::BulletText("%s", project.name.c_str());
        inAnyProject = true;
    }
    if (!inAnyProject) ImGui::TextDisabled("Not in any project");
}

void PreviewPanel::ShowToolResult(const std::string& error) {
    if (error.empty()) return;
    context_.statusMessage = error;
    message_ = error;
}

// ---- Renaming ----

bool PreviewPanel::RenameAsset(const std::string& requestedName, std::string& error) {
    const std::string name = CleanFileName(requestedName, error);
    if (name.empty()) return false;
    if (name == asset_.name) return true;

    // Every file of the asset keeps its extension and gets the new name.
    struct FileMove {
        std::filesystem::path from;
        std::filesystem::path to;
    };
    std::vector<FileMove> moves;
    for (const std::filesystem::path* file : {&asset_.baseImagePath, &asset_.asepritePath, &asset_.audioPath}) {
        if (file->empty()) continue;
        std::filesystem::path target = file->parent_path() / util::Utf8Path(name);
        target += file->extension();
        moves.push_back({*file, target});
    }

    // Only changing letter case (hero -> Hero) points at the "same" file on Windows, which is fine.
    const bool onlyCaseChanged = util::ToLower(name) == asset_.lowercaseName;
    for (const FileMove& move : moves) {
        std::error_code existsError;
        if (!onlyCaseChanged && std::filesystem::exists(move.to, existsError)) {
            error = move.to.filename().string() + " already exists";
            return false;
        }
    }

    std::vector<FileMove> done;
    for (const FileMove& move : moves) {
        std::error_code renameError;
        std::filesystem::rename(move.from, move.to, renameError);
        if (renameError) {
            for (auto undo = done.rbegin(); undo != done.rend(); ++undo) {
                std::error_code ignored;
                std::filesystem::rename(undo->to, undo->from, ignored); // best-effort rollback
            }
            error = "Rename failed: " + renameError.message();
            return false;
        }
        done.push_back(move);
    }

    // Projects, selection and thumbnails all refer to the asset by key, which includes the name.
    const std::string oldKey = asset_.key;
    const std::string newKey = library::KeyAfterRename(asset_, name);
    context_.projects.RenameAssetKey(oldKey, newKey);
    if (context_.selectedKeys.erase(oldKey)) context_.selectedKeys.insert(newKey);
    context_.thumbnails.Forget(oldKey);

    asset_.name = name;
    asset_.lowercaseName = util::ToLower(name);
    asset_.key = newKey;
    for (const FileMove& move : moves) {
        if (move.from == asset_.baseImagePath) {
            asset_.baseImagePath = move.to;
        } else if (move.from == asset_.asepritePath) {
            asset_.asepritePath = move.to;
        } else {
            asset_.audioPath = move.to;
        }
    }
    context_.RescanLibrary();
    return true;
}

} // namespace pixie::views
