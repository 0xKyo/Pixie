#include "app/views/ConverterView.h"

#include <imgui.h>

#include <nfd.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>

#include "app/util/Dialogs.h"
#include "app/util/Gui.h"
#include "app/util/Platform.h"
#include "app/util/Texture.h"
#include "core/formats/ImageFile.h"
#include "core/graphics/PaletteRemap.h"
#include "core/library/Asset.h"

namespace pixie::views {

namespace {

// Where the file dialogs start.
constexpr const nfdnchar_t* kSourceImagesFolder = PIXIE_NSTR("E:\\Workspace\\Pixie\\assets\\source");
constexpr const nfdnchar_t* kConvertedImagesFolder = PIXIE_NSTR("E:\\Workspace\\Pixie\\assets\\converted");

// Loaded at startup so a palette is always active.
constexpr const char* kStartupPaletteFile = "E:\\Workspace\\Pixie\\assets\\palettes\\apollo.gpl";

// Palettes live in a sibling project, so the folder is found relative to the executable.
std::filesystem::path PaletteFolder() {
    const char* executableFolder = SDL_GetBasePath(); // owned by SDL
    const std::filesystem::path base = executableFolder ? std::filesystem::path(executableFolder) : std::filesystem::current_path();
    return (base / ".." / ".." / ".." / ".." / "InerteGames" / "Games" / "Maximo" / "Art" / "Palettes").lexically_normal();
}

std::string PaletteSummary(const color::Palette& palette) {
    return palette.Name() + " (" + std::to_string(palette.Colors().size()) + " colors)";
}

void CopyUtf8(char (&destination)[128], const std::string& text) {
    std::snprintf(destination, sizeof(destination), "%s", text.c_str());
}

std::string StemUtf8(const std::filesystem::path& path) {
    const std::u8string stem = path.stem().u8string();
    return std::string(reinterpret_cast<const char*>(stem.data()), stem.size());
}

} // namespace

// ---- Lifetime and entry points ----

void ConverterView::LoadStartupPalette() {
    std::string error;
    if (palette_.LoadFromGpl(kStartupPaletteFile, &error)) {
        context_.statusMessage = "Loaded palette: " + PaletteSummary(palette_);
    } else {
        context_.statusMessage = "Failed to load default palette: " + error;
    }
}

void ConverterView::Shutdown() {
    if (worker_.valid()) worker_.wait();
    util::DestroyTexture(sourceTexture_);
    util::DestroyTexture(resultTexture_);
}

void ConverterView::OpenAsset(const library::Asset& asset) {
    const std::filesystem::path path = asset.MainFile();
    if (!LoadSourceImage(path)) return;
    batchPaths_ = {path};
    previewedIndex_ = 0;
    batchBesideOriginals_ = false;
    CopyUtf8(batchBaseName_, asset.name);

    if (asset.HasBaseImage() && asset.HasAseprite()) {
        std::string error;
        graphics::Image existing;
        if (formats::LoadImageFile(asset.asepritePath, existing, &error)) {
            ShowResult(std::move(existing));
            context_.statusMessage += " - showing existing .aseprite as the converted version";
        } else {
            context_.statusMessage += " - could not read its .aseprite: " + error;
        }
    }
    context_.screen = app::Screen::Converter;
}

void ConverterView::OpenBatch(std::vector<std::filesystem::path> paths, size_t skippedWithoutBase) {
    if (paths.empty() || !LoadSourceImage(paths.front())) return;
    batchPaths_ = std::move(paths);
    previewedIndex_ = 0;
    batchBesideOriginals_ = true;
    CopyUtf8(batchBaseName_, StemUtf8(batchPaths_.front()));
    if (skippedWithoutBase > 0) {
        context_.statusMessage += " (" + std::to_string(skippedWithoutBase) + " selected asset(s) without a base were skipped)";
    }
    context_.screen = app::Screen::Converter;
}

void ConverterView::Update() {
    if (!converting_ || !worker_.valid()) return;
    if (worker_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    worker_.get();
    converting_ = false;
    FinishConversion();
}

void ConverterView::FinishConversion() {
    if (convertingBatch_) {
        convertingBatch_ = false;
        const std::string destination = batchBesideOriginals_ ? std::string("their original folders") : batchOutputFolder_.string();
        context_.statusMessage = "Converted " + std::to_string(batchProgress_.saved) + " of " + std::to_string(batchTotal_) +
                                 " images to " + destination;
        if (batchProgress_.failed > 0) {
            context_.statusMessage += " (" + std::to_string(batchProgress_.failed) + " failed; first error: " + batchProgress_.firstError + ")";
        }
        if (batchBesideOriginals_) context_.RescanLibrary(); // new .aseprite files appeared in the library
        return;
    }

    ShowResult(std::move(result_));
    const bool usedPalette = tools_.usePalette && !palette_.Empty();
    context_.statusMessage = "Converted to " + std::to_string(resultWidth_) + "x" + std::to_string(resultHeight_);
    context_.statusMessage += usedPalette ? " using palette \"" + palette_.Name() + "\"" : " (original colors, no palette)";
    if (tools_.downsize && usedPalette) context_.statusMessage += " (palette swapped before resize)";
}

// ---- State helpers ----

bool ConverterView::LoadSourceImage(const std::filesystem::path& path) {
    std::string error;
    graphics::Image loaded;
    if (!formats::LoadImageFile(path, loaded, &error)) {
        context_.statusMessage = "Failed to load image " + path.filename().string() + ": " + error;
        return false;
    }
    source_ = std::move(loaded);
    sourcePath_ = path;

    result_.Clear();
    resultWidth_ = 0;
    resultHeight_ = 0;
    util::DestroyTexture(resultTexture_);

    backgroundMask_.Clear();
    if (tools_.removeBackground) RecomputeBackgroundMask();
    samplingBox_ = graphics::CropRect{};
    samplingBoxEditor_.CancelDrag();
    if (IsSamplingBoxActive()) ResetSamplingBox();

    RebuildSourceTexture();
    context_.statusMessage = "Loaded image: " + path.filename().string() + " (" + std::to_string(source_.Width()) + "x" +
                             std::to_string(source_.Height()) + ")";
    return true;
}

void ConverterView::SelectBatchImage(size_t index) {
    if (converting_ || index >= batchPaths_.size() || index == previewedIndex_) return;
    if (LoadSourceImage(batchPaths_[index])) previewedIndex_ = index;
}

void ConverterView::ShowResult(graphics::Image image) {
    result_ = std::move(image);
    resultWidth_ = result_.Width();
    resultHeight_ = result_.Height();
    util::DestroyTexture(resultTexture_);
    resultTexture_ = util::CreateTexture(context_.renderer, result_);
}

// The source preview shows the cheap tools live (transparency and background), not the palette.
void ConverterView::RebuildSourceTexture() {
    util::DestroyTexture(sourceTexture_);
    const bool showMask = tools_.removeBackground && backgroundMask_.MatchesSize(source_.Width(), source_.Height());
    if (!showMask && !tools_.removeTransparency) {
        sourceTexture_ = util::CreateTexture(context_.renderer, source_);
        return;
    }
    graphics::Image preview = source_;
    if (tools_.removeTransparency) graphics::RemoveTransparency(preview);
    if (showMask) graphics::ApplyMask(backgroundMask_, preview);
    sourceTexture_ = util::CreateTexture(context_.renderer, preview);
}

void ConverterView::RecomputeBackgroundMask() {
    if (source_.Empty()) {
        backgroundMask_.Clear();
        return;
    }
    graphics::DetectBackground(source_, tools_.backgroundTolerance, backgroundMask_);
}

void ConverterView::ResetSamplingBox() {
    samplingBox_ = graphics::CenteredRectWithAspect(source_.Width(), source_.Height(), tools_.targetWidth, tools_.targetHeight);
    samplingBoxEditor_.CancelDrag();
}

services::ConversionSettings ConverterView::CurrentSettings() const {
    services::ConversionSettings settings;
    settings.removeTransparency = tools_.removeTransparency;
    settings.resize = tools_.downsize;
    settings.targetWidth = tools_.targetWidth;
    settings.targetHeight = tools_.targetHeight;
    return settings;
}

// ---- Toolbar actions ----

void ConverterView::OpenImagesFromDialog() {
    NFD::UniquePathSet chosen;
    const nfdnfilteritem_t filters[] = {
        {PIXIE_NSTR("Image Files"), PIXIE_NSTR("png,jpg,jpeg,aseprite,ase,bmp,tga,gif,psd,hdr,pic,pgm,ppm")},
    };
    const nfdresult_t result = NFD::OpenDialogMultiple(chosen, filters, 1, kSourceImagesFolder);
    if (result == NFD_ERROR) {
        context_.statusMessage = std::string("File dialog error: ") + NFD::GetError();
        return;
    }
    if (result != NFD_OKAY) return;

    nfdpathsetsize_t count = 0;
    if (NFD::PathSet::Count(chosen, count) != NFD_OKAY || count == 0) return;
    std::vector<std::filesystem::path> paths;
    for (nfdpathsetsize_t i = 0; i < count; ++i) {
        NFD::UniquePathSetPathN path;
        if (NFD::PathSet::GetPath(chosen, i, path) == NFD_OKAY) paths.emplace_back(path.get());
    }

    // The current batch is only replaced if the first new image loads.
    if (paths.empty() || !LoadSourceImage(paths.front())) return;
    batchPaths_ = std::move(paths);
    previewedIndex_ = 0;
    batchBesideOriginals_ = false;
    CopyUtf8(batchBaseName_, StemUtf8(batchPaths_.front()));
    if (batchPaths_.size() > 1) {
        context_.statusMessage += " - " + std::to_string(batchPaths_.size()) + " images in batch";
    }
}

void ConverterView::OpenPaletteFromDialog() {
    NFD::UniquePathN chosen;
    const nfdnfilteritem_t filters[] = {{PIXIE_NSTR("GIMP Palette"), PIXIE_NSTR("gpl")}};
    const std::filesystem::path startFolder = PaletteFolder();
    const nfdresult_t result = NFD::OpenDialog(chosen, filters, 1, startFolder.c_str());
    if (result == NFD_ERROR) {
        context_.statusMessage = std::string("File dialog error: ") + NFD::GetError();
        return;
    }
    if (result != NFD_OKAY) return;

    std::string error;
    color::Palette loaded;
    if (loaded.LoadFromGpl(std::filesystem::path(chosen.get()), &error)) {
        palette_ = std::move(loaded);
        context_.statusMessage = "Loaded palette: " + PaletteSummary(palette_);
    } else {
        context_.statusMessage = "Failed to load palette: " + error;
    }
}

void ConverterView::SaveResultFromDialog() {
    if (result_.Empty()) return;

    const bool asAseprite = saveAsAseprite_;
    const nfdnfilteritem_t filters[] = {asAseprite ? nfdnfilteritem_t{PIXIE_NSTR("Aseprite"), PIXIE_NSTR("aseprite")}
                                                   : nfdnfilteritem_t{PIXIE_NSTR("PNG Image"), PIXIE_NSTR("png")}};
    const util::NativeString extension = asAseprite ? PIXIE_NSTR(".aseprite") : PIXIE_NSTR(".png");
    const util::NativeString suggestedName =
        (sourcePath_.empty() ? util::NativeString(PIXIE_NSTR("converted")) : sourcePath_.stem().native()) + extension;

    NFD::UniquePathN chosen;
    const nfdresult_t result = NFD::SaveDialog(chosen, filters, 1, kConvertedImagesFolder, suggestedName.c_str());
    if (result == NFD_ERROR) {
        context_.statusMessage = std::string("File dialog error: ") + NFD::GetError();
        return;
    }
    if (result != NFD_OKAY) return;

    const std::filesystem::path path(chosen.get());
    std::string error;
    const bool saved = asAseprite ? formats::SaveAseprite(result_, path, &error) : formats::SavePng(result_, path, &error);
    context_.statusMessage = saved ? "Saved: " + path.filename().string() : "Failed to save: " + error;
}

void ConverterView::SaveResultBesideOriginal() {
    if (result_.Empty() || sourcePath_.empty() || library::IsAsepriteFile(sourcePath_)) return;
    const std::filesystem::path destination = library::AsepritePathFor(sourcePath_);
    std::error_code existsError;
    if (std::filesystem::exists(destination, existsError) && !util::ConfirmOverwrite(context_.window, 1)) return;

    std::string error;
    if (formats::SaveAseprite(result_, destination, &error)) {
        context_.statusMessage = "Saved: " + destination.filename().string();
        context_.RescanLibrary();
    } else {
        context_.statusMessage = "Failed to save: " + error;
    }
}

void ConverterView::ConvertPreviewedImage() {
    if (source_.Empty() || converting_) return;
    converting_ = true;
    context_.statusMessage = "Converting...";

    // Copy everything the worker needs: the UI may change these the moment this returns.
    const bool useCrop = IsSamplingBoxActive() && !samplingBox_.Empty();
    const graphics::CropRect crop = useCrop ? samplingBox_ : graphics::CropRect{};
    const bool useMask = tools_.removeBackground && backgroundMask_.MatchesSize(source_.Width(), source_.Height());
    auto source = std::make_shared<graphics::Image>(source_);
    auto mask = useMask ? std::make_shared<graphics::Mask>(backgroundMask_) : nullptr;
    const services::ConversionSettings settings = CurrentSettings();
    const bool usePalette = tools_.usePalette && !palette_.Empty(); // the palette can't change while converting

    worker_ = std::async(std::launch::async, [this, source, mask, settings, crop, usePalette]() {
        result_ = services::ConvertImage(*source, settings, mask.get(), crop, usePalette ? &palette_ : nullptr);
    });
}

void ConverterView::ConvertAllImages() {
    if (batchPaths_.empty() || converting_) return;

    const bool besideOriginals = batchBesideOriginals_;
    if (besideOriginals) {
        int alreadyConverted = 0;
        for (const std::filesystem::path& path : batchPaths_) {
            std::error_code existsError;
            if (!library::IsAsepriteFile(path) && std::filesystem::exists(library::AsepritePathFor(path), existsError)) {
                ++alreadyConverted;
            }
        }
        if (alreadyConverted > 0 && !util::ConfirmOverwrite(context_.window, alreadyConverted)) return;
    }

    std::filesystem::path outputFolder;
    if (!besideOriginals) {
        NFD::UniquePathN chosen;
        const nfdresult_t result = NFD::PickFolder(chosen, kConvertedImagesFolder);
        if (result == NFD_ERROR) {
            context_.statusMessage = std::string("File dialog error: ") + NFD::GetError();
            return;
        }
        if (result != NFD_OKAY) return;
        outputFolder = std::filesystem::path(chosen.get());
    }

    services::BatchJob job;
    job.sources = batchPaths_;
    job.settings = CurrentSettings();
    if (tools_.usePalette && !palette_.Empty()) job.palette = palette_;
    job.cropToTargetAspect = IsSamplingBoxActive();
    job.previewCrop = samplingBox_;
    job.previewWidth = source_.Width();
    job.previewHeight = source_.Height();
    job.previewIndex = previewedIndex_;
    if (tools_.removeBackground && backgroundMask_.MatchesSize(source_.Width(), source_.Height())) {
        job.previewMask = backgroundMask_; // keeps the user's manual include/exclude clicks
    }
    job.removeBackground = tools_.removeBackground;
    job.backgroundTolerance = tools_.backgroundTolerance;
    job.naming = besideOriginals ? services::OutputNaming::BesideOriginal
                 : batchKeepNames_ ? services::OutputNaming::KeepName
                                   : services::OutputNaming::Numbered;
    job.outputFolder = outputFolder;
    job.baseName = batchBaseName_[0] ? batchBaseName_ : "converted";
    job.saveAsAseprite = saveAsAseprite_ || besideOriginals; // beside a PNG we never write a PNG

    converting_ = true;
    convertingBatch_ = true;
    batchTotal_ = static_cast<int>(batchPaths_.size());
    batchOutputFolder_ = outputFolder;
    batchProgress_.Reset();
    context_.statusMessage = "Converting...";
    worker_ = std::async(std::launch::async, [this, job = std::move(job)]() { services::RunBatch(job, batchProgress_); });
}

// ---- Layout ----

void ConverterView::Render() {
    util::BeginFullscreenWindow("Converter");
    RenderToolbar();
    if (!context_.statusMessage.empty()) ImGui::TextWrapped("%s", context_.statusMessage.c_str());
    ImGui::Separator();

    RenderSidebar();
    ImGui::SameLine();
    ImGui::BeginChild("Previews", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f - 4.0f;
    RenderSourcePreview(halfWidth);
    ImGui::SameLine();
    RenderResultPreview();
    ImGui::EndChild();

    ImGui::End();
}

void ConverterView::RenderToolbar() {
    ImGui::BeginDisabled(converting_ || context_.settings.libraryRoot.empty());
    if (ImGui::Button("< Library")) context_.screen = app::Screen::Library;
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(converting_);
    if (ImGui::Button("Open Image(s)...")) OpenImagesFromDialog();
    ImGui::SameLine();
    if (ImGui::Button("Open Palette (.gpl)...")) OpenPaletteFromDialog();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(source_.Empty() || converting_);
    if (ImGui::Button("Convert")) ConvertPreviewedImage();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(converting_ || !HasResult());
    if (ImGui::Button(saveAsAseprite_ ? "Save Result As Aseprite..." : "Save Result As PNG...")) SaveResultFromDialog();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(converting_ || !HasResult() || sourcePath_.empty() || library::IsAsepriteFile(sourcePath_));
    if (ImGui::Button("Save Beside Original")) SaveResultBesideOriginal();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Writes the result as <original name>.aseprite in the original's folder");
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(batchPaths_.size() < 2 || converting_);
    const std::string imageCount = std::to_string(batchPaths_.size());
    const std::string convertAllLabel = batchBesideOriginals_ ? "Convert All (" + imageCount + ") Beside Originals"
                                                              : "Convert All (" + imageCount + ") To Folder...";
    if (ImGui::Button(convertAllLabel.c_str())) ConvertAllImages();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(converting_);
    ImGui::Checkbox("Convert To Aseprite", &saveAsAseprite_);
    ImGui::EndDisabled();

    if (converting_) {
        ImGui::SameLine();
        if (convertingBatch_) {
            ImGui::Text("Converting %d/%d...", batchProgress_.processed.load(), batchTotal_);
        } else {
            ImGui::TextUnformatted("Converting...");
        }
    }
}

void ConverterView::RenderSidebar() {
    constexpr float kSidebarWidth = 240.0f;
    ImGui::BeginChild("Sidebar", ImVec2(kSidebarWidth, 0.0f), ImGuiChildFlags_Borders);
    RenderPalettePanel();
    if (batchPaths_.size() > 1) RenderBatchPanel();
    RenderToolsPanel();
    ImGui::EndChild();
}

void ConverterView::RenderPalettePanel() {
    ImGui::BeginDisabled(converting_);
    ImGui::Checkbox("Use Palette", &tools_.usePalette);
    ImGui::EndDisabled();
    ImGui::Text("Palette: %s", palette_.Empty() ? "(none)" : palette_.Name().c_str());
    ImGui::Text("%zu colors", palette_.Colors().size());
    ImGui::Separator();

    // A fixed number of rows; bigger palettes scroll sideways instead of making the column taller.
    constexpr int kSwatchRows = 5;
    constexpr float kSwatchSize = 20.0f;
    constexpr float kSwatchStep = kSwatchSize + 4.0f;
    const float gridHeight = kSwatchRows * kSwatchStep + ImGui::GetStyle().ScrollbarSize;
    ImGui::BeginChild("Swatches", ImVec2(0.0f, gridHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 gridOrigin = ImGui::GetCursorPos();
    int swatchIndex = 0;
    for (const color::PaletteColor& entry : palette_.Colors()) {
        const int row = swatchIndex % kSwatchRows;
        const int column = swatchIndex / kSwatchRows;
        ImGui::SetCursorPos(ImVec2(gridOrigin.x + column * kSwatchStep, gridOrigin.y + row * kSwatchStep));
        ImGui::PushID(swatchIndex);
        const ImVec4 swatchColor(entry.rgb.r / 255.0f, entry.rgb.g / 255.0f, entry.rgb.b / 255.0f, 1.0f);
        ImGui::ColorButton("##swatch", swatchColor, ImGuiColorEditFlags_NoTooltip, ImVec2(kSwatchSize, kSwatchSize));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nRGB(%d, %d, %d)", entry.name.empty() ? "(unnamed)" : entry.name.c_str(), entry.rgb.r,
                              entry.rgb.g, entry.rgb.b);
        }
        ImGui::PopID();
        ++swatchIndex;
    }
    ImGui::EndChild();
}

void ConverterView::RenderBatchPanel() {
    ImGui::Separator();
    ImGui::Text("Images (%zu)", batchPaths_.size());
    ImGui::BeginChild("BatchImages", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders);
    ImGui::BeginDisabled(converting_);
    for (size_t i = 0; i < batchPaths_.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(batchPaths_[i].filename().string().c_str(), i == previewedIndex_)) SelectBatchImage(i);
        ImGui::PopID();
    }
    ImGui::EndDisabled();
    ImGui::EndChild();

    ImGui::BeginDisabled(converting_);
    ImGui::Checkbox("Save beside originals (.aseprite)", &batchBesideOriginals_);
    ImGui::BeginDisabled(batchBesideOriginals_);
    ImGui::Checkbox("Keep Name", &batchKeepNames_);
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (batchBesideOriginals_) {
        ImGui::TextDisabled("Output: <name>.aseprite next to each image");
    } else if (batchKeepNames_) {
        ImGui::TextDisabled("Output: each image keeps its own name");
    } else {
        ImGui::TextUnformatted("Asset name");
        ImGui::BeginDisabled(converting_);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##BatchBaseName", batchBaseName_, sizeof(batchBaseName_));
        ImGui::EndDisabled();
        const char* shownName = batchBaseName_[0] ? batchBaseName_ : "converted";
        ImGui::TextDisabled("Output: %s_01, %s_02, ...", shownName, shownName);
    }
    ImGui::TextDisabled("Tool settings apply to all images");
}

void ConverterView::RenderToolsPanel() {
    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    ImGui::Spacing();

    ImGui::BeginDisabled(converting_ || source_.Empty());

    if (ImGui::Checkbox("Remove Transparency", &tools_.removeTransparency)) RebuildSourceTexture();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Checkbox("Remove Background", &tools_.removeBackground)) {
        if (tools_.removeBackground && !backgroundMask_.MatchesSize(source_.Width(), source_.Height())) RecomputeBackgroundMask();
        RebuildSourceTexture();
    }
    if (tools_.removeBackground) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##BackgroundTolerance", &tools_.backgroundTolerance, 1.0f, 30.0f, "Tolerance %.1f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { // recomputing is slow, so wait until the slider is released
            RecomputeBackgroundMask();
            RebuildSourceTexture();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderDownsizeTool();
    ImGui::EndDisabled();
}

void ConverterView::RenderDownsizeTool() {
    if (ImGui::Checkbox("Downsize", &tools_.downsize)) {
        samplingBoxEditor_.CancelDrag();
        if (IsSamplingBoxActive() && samplingBox_.Empty()) ResetSamplingBox();
    }
    if (!tools_.downsize) return;

    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("Width", &tools_.targetWidth);
    tools_.targetWidth = std::max(1, tools_.targetWidth);
    const bool widthEdited = ImGui::IsItemDeactivatedAfterEdit();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("Height", &tools_.targetHeight);
    tools_.targetHeight = std::max(1, tools_.targetHeight);
    const bool heightEdited = ImGui::IsItemDeactivatedAfterEdit();

    // A new target size means a new aspect ratio, so the box starts over.
    if ((widthEdited || heightEdited) && tools_.keepAspect) ResetSamplingBox();

    if (ImGui::Checkbox("Maintain Ratio", &tools_.keepAspect)) {
        if (tools_.keepAspect && samplingBox_.Empty()) ResetSamplingBox();
        samplingBoxEditor_.CancelDrag();
    }
    if (!tools_.keepAspect) {
        ImGui::TextDisabled("Image will be stretched to fit");
        return;
    }
    if (ImGui::Button("Reset Sampling")) ResetSamplingBox();
    ImGui::SameLine();
    ImGui::Text("(%d, %d) %dx%d", samplingBox_.x, samplingBox_.y, samplingBox_.width, samplingBox_.height);
}

void ConverterView::RenderSourcePreview(float width) {
    ImGui::BeginChild("SourcePreview", ImVec2(width, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text("Source (%dx%d)", source_.Width(), source_.Height());
    if (IsSamplingBoxActive()) {
        ImGui::TextDisabled("Drag the handles to choose the sampled region");
    } else if (tools_.removeBackground) {
        ImGui::TextDisabled("Click the image to include/exclude a region");
    }

    if (sourceTexture_) {
        ImGui::Image(util::ToImGui(sourceTexture_), util::FitInside(source_.Width(), source_.Height(), ImGui::GetContentRegionAvail()));
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageSize = ImGui::GetItemRectSize();
        const bool imageVisible = imageSize.x > 0.0f && imageSize.y > 0.0f;

        if (IsSamplingBoxActive() && !converting_ && imageVisible) {
            const double targetAspect = static_cast<double>(tools_.targetWidth) / tools_.targetHeight;
            samplingBoxEditor_.Update(samplingBox_, source_.Width(), source_.Height(), imageMin, imageSize, targetAspect);
        } else if (tools_.removeBackground && !converting_ && !backgroundMask_.Empty() && imageVisible &&
                   ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            // Map the click from screen space to a source pixel and flip that region.
            const ImVec2 mouse = ImGui::GetMousePos();
            const int pixelX = std::clamp(static_cast<int>((mouse.x - imageMin.x) / imageSize.x * source_.Width()), 0, source_.Width() - 1);
            const int pixelY = std::clamp(static_cast<int>((mouse.y - imageMin.y) / imageSize.y * source_.Height()), 0, source_.Height() - 1);
            graphics::ToggleRegion(source_, pixelX, pixelY, tools_.backgroundTolerance, backgroundMask_);
            RebuildSourceTexture();
        }
    }
    ImGui::EndChild();
}

void ConverterView::RenderResultPreview() {
    ImGui::BeginChild("ResultPreview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    ImGui::Text("Converted (%dx%d)", resultWidth_, resultHeight_);
    if (resultTexture_) {
        ImGui::Image(util::ToImGui(resultTexture_), util::FitInside(resultWidth_, resultHeight_, ImGui::GetContentRegionAvail()));
    }
    ImGui::EndChild();
}

} // namespace pixie::views
