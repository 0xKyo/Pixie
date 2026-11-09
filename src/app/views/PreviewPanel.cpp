#include "app/views/PreviewPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "app/services/ExternalTools.h"
#include "app/util/Gui.h"
#include "app/util/Text.h"
#include "app/util/Texture.h"
#include "core/formats/BinaryFile.h"
#include "core/formats/ImageFile.h"

namespace pixie::views {

namespace {

constexpr float kImagePaneHeight = 240.0f;
constexpr float kWaveformHeight = 120.0f;
constexpr int kWaveformColumns = 1024;
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

// The waveform (click or drag to seek) with a playhead, then the transport buttons.
void PreviewPanel::RenderWaveform() {
    player_.Update();
    const std::uint64_t frameCount = clip_.FrameCount();
    const std::uint64_t position = player_.Position();

    const ImVec2 size(ImGui::GetContentRegionAvail().x, kWaveformHeight);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##waveform", size);
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
    const float playheadX = origin.x + size.x * static_cast<float>(position) / static_cast<float>(std::max<std::uint64_t>(1, frameCount));
    drawList->AddLine(ImVec2(playheadX, origin.y), ImVec2(playheadX, origin.y + size.y), IM_COL32(255, 200, 80, 255), 2.0f);

    if (player_.HasDevice() && ImGui::IsItemActive()) {
        const float fraction = std::clamp((ImGui::GetMousePos().x - origin.x) / size.x, 0.0f, 1.0f);
        player_.Seek(static_cast<std::uint64_t>(fraction * static_cast<float>(frameCount)));
    }

    ImGui::BeginDisabled(!player_.HasDevice());
    if (ImGui::Button(player_.IsPlaying() ? "Pause" : "Play")) player_.TogglePlay();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) player_.Stop();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%s / %s", util::FormatDuration(static_cast<double>(position) / clip_.sampleRate).c_str(),
                util::FormatDuration(clip_.DurationSeconds()).c_str());
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
