#include "app/views/LibraryView.h"

#include <nfd.hpp>

#include <algorithm>
#include <cmath>

#include "app/services/ExternalTools.h"
#include "app/services/SpriteSheetExport.h"
#include "app/util/Gui.h"
#include "app/util/Text.h"

namespace pixie::views {

namespace {

constexpr float kMinTileSize = 80.0f;
constexpr float kMaxTileSize = 256.0f;
constexpr float kTileSpacing = 8.0f;
constexpr float kTilePadding = 4.0f;
constexpr float kPreviewPanelWidth = 400.0f;

// Tile colors.
constexpr ImU32 kTileColor = IM_COL32(42, 42, 48, 255);
constexpr ImU32 kTileHoveredColor = IM_COL32(58, 58, 68, 255);
constexpr ImU32 kTileSelectedColor = IM_COL32(52, 84, 150, 255);
constexpr ImU32 kSelectedBorderColor = IM_COL32(110, 160, 255, 255);
constexpr ImU32 kPreviewedBorderColor = IM_COL32(255, 200, 80, 255);
constexpr ImU32 kImageBackgroundColor = IM_COL32(24, 24, 28, 255);
constexpr ImU32 kVariationColor = IM_COL32(30, 140, 140, 255);
constexpr ImU32 kWaveColor = IM_COL32(110, 170, 255, 255);

// Chip colors.
constexpr ImU32 kBaseFormatChipColor = IM_COL32(70, 70, 80, 230);
constexpr ImU32 kAudioFormatChipColor = IM_COL32(150, 90, 40, 230);
constexpr ImU32 kAsepriteChipColor = IM_COL32(40, 130, 80, 230);
constexpr ImU32 kInProjectChipColor = IM_COL32(60, 100, 190, 230);

// A decorative "sound wave" for audio tiles, seeded by the name so each file looks different.
void DrawAudioIcon(ImDrawList* drawList, const std::string& name, ImVec2 areaMin, float areaSize) {
    unsigned seed = 2166136261u; // FNV-1a hash of the name
    for (const char c : name) seed = (seed ^ static_cast<unsigned char>(c)) * 16777619u;

    constexpr int kBarCount = 17;
    const float barWidth = areaSize / static_cast<float>(kBarCount * 2);
    const float centerY = areaMin.y + areaSize * 0.5f;
    for (int bar = 0; bar < kBarCount; ++bar) {
        seed = seed * 1664525u + 1013904223u; // simple pseudo-random step
        const float randomness = static_cast<float>((seed >> 16) & 0xFF) / 255.0f;
        const float envelope = 1.0f - std::fabs(static_cast<float>(bar) - kBarCount * 0.5f) / (kBarCount * 0.5f);
        const float height = areaSize * (0.12f + 0.5f * envelope * (0.3f + 0.7f * randomness));
        const float centerX = areaMin.x + barWidth * (bar * 2 + 1) + barWidth * 0.5f;
        drawList->AddRectFilled(ImVec2(centerX - barWidth * 0.5f, centerY - height * 0.5f),
                                ImVec2(centerX + barWidth * 0.5f, centerY + height * 0.5f), kWaveColor);
    }
}

} // namespace

// ---- Settings and the first-run screen ----

void LibraryView::LoadSettings() {
    const float saved = context_.settings.tileSize;
    if (saved >= kMinTileSize && saved <= kMaxTileSize) tileSize_ = saved;
}

void LibraryView::SaveSettings() {
    if (context_.settings.libraryRoot.empty()) return;
    context_.settings.tileSize = tileSize_;
    context_.settings.Save();
}

void LibraryView::RenderFolderPicker() {
    util::BeginFullscreenWindow("FolderPicker");
    const ImVec2 available = ImGui::GetContentRegionAvail();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + available.y * 0.3f);
    auto centeredText = [&](const char* text) {
        ImGui::SetCursorPosX((available.x - ImGui::CalcTextSize(text).x) * 0.5f);
        ImGui::TextUnformatted(text);
    };
    centeredText("Welcome to Pixie");
    ImGui::Spacing();
    centeredText("Choose the folder where your assets live.");
    centeredText("Pixie will browse every .png, .jpg and .aseprite inside it.");
    ImGui::Spacing();
    ImGui::Spacing();

    constexpr float kButtonWidth = 220.0f;
    ImGui::SetCursorPosX((available.x - kButtonWidth) * 0.5f);
    if (ImGui::Button("Choose Assets Folder...", ImVec2(kButtonWidth, 36.0f))) PickLibraryFolder();
    if (!context_.statusMessage.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", context_.statusMessage.c_str());
    }
    ImGui::End();
}

// ---- Main screen ----

void LibraryView::Render() {
    context_.thumbnails.BeginFrame();
    util::BeginFullscreenWindow("Library");

    RenderToolbar();
    RenderFilterBar();
    RenderProjectBar();

    const std::vector<int> visible = VisibleAssetIndices();
    const std::vector<library::GridTile> tiles = library::BuildGridTiles(context_.assets, visible, expandedGroups_);

    ImGui::TextDisabled("%zu of %zu assets  |  click to open, Ctrl+click to select several, double-click a stack to expand it",
                        visible.size(), context_.assets.size());
    if (!context_.statusMessage.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted("-");
        ImGui::SameLine();
        ImGui::TextUnformatted(context_.statusMessage.c_str());
    }
    ImGui::Separator();

    const float gridWidth = preview_.IsOpen() ? ImGui::GetContentRegionAvail().x - kPreviewPanelWidth - 8.0f : 0.0f;
    ImGui::BeginChild("AssetGrid", ImVec2(gridWidth, 0.0f), ImGuiChildFlags_None);
    if (context_.assets.empty()) ImGui::TextWrapped("No .png, .jpg or .aseprite files found in this folder.");
    RenderGrid(tiles);
    ImGui::EndChild();

    if (preview_.IsOpen()) {
        ImGui::SameLine();
        ImGui::BeginChild("PreviewPanel", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        preview_.Render();
        ImGui::EndChild();
    }
    ImGui::End();
}

void LibraryView::RenderToolbar() {
    if (ImGui::Button("Change Folder...")) PickLibraryFolder();
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) context_.RescanLibrary();
    ImGui::SameLine();
    if (ImGui::Button("Converter")) context_.screen = app::Screen::Converter;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open the converter without a specific asset");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", context_.settings.libraryRoot.string().c_str());
}

void LibraryView::RenderFilterBar() {
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##Search", "Search...", searchText_, sizeof(searchText_));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    const char* conversionOptions[] = {"All", "Converted", "Not converted"};
    int conversionChoice = static_cast<int>(conversionFilter_);
    if (ImGui::Combo("##Conversion", &conversionChoice, conversionOptions, 3)) {
        conversionFilter_ = static_cast<ConversionFilter>(conversionChoice);
    }

    ImGui::SameLine();
    RenderTypeFilter();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("##TileSize", &tileSize_, kMinTileSize, kMaxTileSize, "Size %.0f");
    if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();

    ImGui::SameLine();
    ImGui::BeginDisabled(context_.selectedKeys.empty());
    const std::string convertLabel = "Convert Selected (" + std::to_string(context_.selectedKeys.size()) + ")";
    if (ImGui::Button(convertLabel.c_str())) ConvertSelected();
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection")) context_.selectedKeys.clear();
    ImGui::EndDisabled();
}

// A dropdown with one checkbox per asset kind; its label summarizes what's checked.
void LibraryView::RenderTypeFilter() {
    int checkedCount = 0;
    int lastChecked = 0;
    for (int kind = 0; kind < library::kAssetKindCount; ++kind) {
        if (!kindVisible_[kind]) continue;
        ++checkedCount;
        lastChecked = kind;
    }
    std::string label;
    if (checkedCount == library::kAssetKindCount) {
        label = "All types";
    } else if (checkedCount == 0) {
        label = "No types";
    } else if (checkedCount == 1) {
        label = library::KindLabel(static_cast<library::AssetKind>(lastChecked));
    } else {
        label = std::to_string(checkedCount) + " types";
    }

    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::BeginCombo("##Types", label.c_str())) {
        for (int kind = 0; kind < library::kAssetKindCount; ++kind) {
            ImGui::Checkbox(library::KindLabel(static_cast<library::AssetKind>(kind)), &kindVisible_[kind]);
        }
        ImGui::EndCombo();
    }
}

void LibraryView::RenderProjectBar() {
    services::ProjectStore& projects = context_.projects;
    const std::string activeName = projects.HasActive() ? projects.Active().name : "Whole library";

    ImGui::TextUnformatted("Project:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##Project", activeName.c_str())) {
        if (ImGui::Selectable("Whole library", !projects.HasActive())) projects.SelectProject(-1);
        for (int index = 0; index < static_cast<int>(projects.All().size()); ++index) {
            const services::Project& project = projects.All()[index];
            const std::string label = project.name + " (" + std::to_string(project.assetKeys.size()) + ")";
            ImGui::PushID(index);
            if (ImGui::Selectable(label.c_str(), projects.ActiveIndex() == index)) {
                projects.SelectProject(index);
                projectScope_ = ProjectScope::OnlyProjectAssets;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    RenderNewProjectButton();
    if (!projects.HasActive()) return;

    ImGui::SameLine();
    ImGui::SetNextItemWidth(190.0f);
    const char* scopeOptions[] = {"Only project assets", "Whole library (pick assets)"};
    int scopeChoice = static_cast<int>(projectScope_);
    if (ImGui::Combo("##Scope", &scopeChoice, scopeOptions, 2)) projectScope_ = static_cast<ProjectScope>(scopeChoice);

    ImGui::SameLine();
    ImGui::BeginDisabled(context_.selectedKeys.empty());
    if (ImGui::Button("Add Selected")) projects.AddToActive(context_.selectedKeys);
    ImGui::SameLine();
    if (ImGui::Button("Remove Selected")) projects.RemoveFromActive(context_.selectedKeys);
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Export Spritesheet...")) ExportSpriteSheet();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Packs every converted (.aseprite) asset of the project into one atlas PNG\n"
                          "plus a layout JSON (positions, durations, tags) for the Godot importer");
    }
    ImGui::SameLine();
    RenderDeleteProjectButton();
}

void LibraryView::RenderNewProjectButton() {
    if (ImGui::Button("New Project...")) {
        newProjectName_[0] = '\0';
        ImGui::OpenPopup("NewProject");
    }
    if (!ImGui::BeginPopup("NewProject")) return;

    ImGui::TextUnformatted("Project name");
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    const bool pressedEnter = ImGui::InputText("##NewProjectName", newProjectName_, sizeof(newProjectName_),
                                               ImGuiInputTextFlags_EnterReturnsTrue);
    const bool hasName = newProjectName_[0] != '\0';
    if ((ImGui::Button("Create") || pressedEnter) && hasName) {
        context_.projects.CreateAndSelect(newProjectName_);
        projectScope_ = ProjectScope::WholeLibrary; // a new project is empty, so start by picking assets
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void LibraryView::RenderDeleteProjectButton() {
    if (ImGui::Button("Delete Project")) ImGui::OpenPopup("DeleteProject");
    if (!ImGui::BeginPopup("DeleteProject")) return;

    ImGui::Text("Delete \"%s\"? (assets are not touched)", context_.projects.Active().name.c_str());
    if (ImGui::Button("Delete")) {
        context_.projects.DeleteActive();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---- Grid ----

std::vector<int> LibraryView::VisibleAssetIndices() const {
    const std::string search = util::ToLower(searchText_);
    const bool onlyProjectAssets = context_.projects.HasActive() && projectScope_ == ProjectScope::OnlyProjectAssets;

    std::vector<int> visible;
    visible.reserve(context_.assets.size());
    for (int index = 0; index < static_cast<int>(context_.assets.size()); ++index) {
        const library::Asset& asset = context_.assets[index];
        if (onlyProjectAssets && !context_.projects.ActiveContains(asset.key)) continue;
        if (!kindVisible_[static_cast<int>(asset.kind)]) continue;
        if (conversionFilter_ != ConversionFilter::All) {
            if (asset.IsAudio()) continue; // "converted" only means something for images
            if ((conversionFilter_ == ConversionFilter::Converted) != asset.HasAseprite()) continue;
        }
        const bool matchesSearch = search.empty() || asset.lowercaseName.find(search) != std::string::npos ||
                                   asset.folder.find(search) != std::string::npos;
        if (!matchesSearch) continue;
        visible.push_back(index);
    }
    return visible;
}

// Tiles flow left to right; the clipper skips rows that are scrolled out of view.
void LibraryView::RenderGrid(const std::vector<library::GridTile>& tiles) {
    const float tileHeight = tileSize_ + ImGui::GetTextLineHeight() + 8.0f;
    const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + kTileSpacing) / (tileSize_ + kTileSpacing)));
    const int rows = static_cast<int>((tiles.size() + columns - 1) / columns);

    ImGuiListClipper clipper;
    clipper.Begin(rows, tileHeight + kTileSpacing);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            for (int column = 0; column < columns; ++column) {
                const size_t tileIndex = static_cast<size_t>(row) * columns + column;
                if (tileIndex >= tiles.size()) break;
                RenderTile(tiles[tileIndex], ImVec2(rowStart.x + column * (tileSize_ + kTileSpacing), rowStart.y));
            }
            ImGui::SetCursorScreenPos(rowStart);
            ImGui::Dummy(ImVec2(1.0f, tileHeight)); // advances the layout by one row
        }
    }
    clipper.End();
}

void LibraryView::RenderTile(const library::GridTile& tile, ImVec2 topLeft) {
    const library::Asset& asset = context_.assets[tile.assetIndex];
    const std::set<std::string> keys = KeysOf(tile);
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImVec2 bottomRight(topLeft.x + tileSize_, topLeft.y + tileSize_ + lineHeight + 8.0f);

    ImGui::SetCursorScreenPos(topLeft);
    ImGui::PushID(tile.assetIndex);
    const bool clicked = ImGui::InvisibleButton("##tile", ImVec2(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y));
    const bool hovered = ImGui::IsItemHovered();
    const bool selected = std::all_of(keys.begin(), keys.end(), [&](const std::string& key) { return context_.selectedKeys.count(key) > 0; });

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (tile.isCollapsedStack) { // two cards peeking out behind the tile
        drawList->AddRectFilled(ImVec2(topLeft.x + 6, topLeft.y + 6), ImVec2(bottomRight.x + 6, bottomRight.y + 6), IM_COL32(34, 34, 40, 255), 4.0f);
        drawList->AddRectFilled(ImVec2(topLeft.x + 3, topLeft.y + 3), ImVec2(bottomRight.x + 3, bottomRight.y + 3), IM_COL32(38, 38, 44, 255), 4.0f);
    }
    drawList->AddRectFilled(topLeft, bottomRight, selected ? kTileSelectedColor : hovered ? kTileHoveredColor : kTileColor, 4.0f);
    if (selected) drawList->AddRect(topLeft, bottomRight, kSelectedBorderColor, 4.0f, 0, 2.0f);
    if (preview_.IsOpen() && asset.key == preview_.CurrentAsset().key) {
        drawList->AddRect(topLeft, bottomRight, kPreviewedBorderColor, 4.0f, 0, 2.0f);
    }
    if (tile.IsInGroup() && !tile.isCollapsedStack) { // a variation of an expanded group
        drawList->AddRectFilled(topLeft, ImVec2(topLeft.x + 3.0f, bottomRight.y), kVariationColor, 2.0f);
    }

    const float areaSize = tileSize_ - 2 * kTilePadding;
    const ImVec2 areaMin(topLeft.x + kTilePadding, topLeft.y + kTilePadding);
    DrawTileImage(asset, areaMin, areaSize);
    DrawTileChips(tile, keys, areaMin, areaSize);

    const std::string& label = tile.isCollapsedStack ? tile.stackLabel : asset.name;
    drawList->PushClipRect(ImVec2(topLeft.x + kTilePadding, topLeft.y), ImVec2(bottomRight.x - kTilePadding, bottomRight.y), true);
    drawList->AddText(ImVec2(topLeft.x + 6.0f, topLeft.y + tileSize_ + 2.0f), IM_COL32(230, 230, 235, 255), label.c_str());
    drawList->PopClipRect();

    if (hovered) RenderTileTooltip(asset);
    RenderTileMenu(tile, keys);
    HandleTileClicks(tile, keys, clicked, hovered, selected);
    ImGui::PopID();
}

void LibraryView::DrawTileImage(const library::Asset& asset, ImVec2 areaMin, float areaSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(areaMin, ImVec2(areaMin.x + areaSize, areaMin.y + areaSize), kImageBackgroundColor);
    if (asset.IsAudio()) {
        DrawAudioIcon(drawList, asset.name, areaMin, areaSize); // decoding audio while scrolling would be too slow
        return;
    }

    const services::Thumbnail& thumbnail = context_.thumbnails.Get(context_.renderer, asset);
    if (thumbnail.texture) {
        const ImVec2 drawn = util::FitInsidePixelPerfect(thumbnail.width, thumbnail.height, ImVec2(areaSize, areaSize));
        const ImVec2 imageMin(areaMin.x + (areaSize - drawn.x) * 0.5f, areaMin.y + (areaSize - drawn.y) * 0.5f);
        drawList->AddImage(util::ToImGui(thumbnail.texture), imageMin, ImVec2(imageMin.x + drawn.x, imageMin.y + drawn.y));
    } else if (thumbnail.failed) {
        drawList->AddText(ImVec2(areaMin.x + 6.0f, areaMin.y + 6.0f), IM_COL32(200, 90, 90, 255), "unreadable");
    }
}

// Small labels: file formats and project membership at the bottom-left, the stack size at the top-right.
void LibraryView::DrawTileChips(const library::GridTile& tile, const std::set<std::string>& keys, ImVec2 areaMin, float areaSize) {
    const library::Asset& asset = context_.assets[tile.assetIndex];
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float lineHeight = ImGui::GetTextLineHeight();

    float chipX = areaMin.x + 3.0f;
    const float chipY = areaMin.y + areaSize - lineHeight - 5.0f;
    auto addChip = [&](const std::string& text, ImU32 color) {
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        drawList->AddRectFilled(ImVec2(chipX, chipY), ImVec2(chipX + textSize.x + 8.0f, chipY + lineHeight + 2.0f), color, 3.0f);
        drawList->AddText(ImVec2(chipX + 4.0f, chipY + 1.0f), IM_COL32(255, 255, 255, 255), text.c_str());
        chipX += textSize.x + 12.0f;
    };
    if (asset.HasBaseImage()) addChip(asset.baseImageFormat, kBaseFormatChipColor);
    if (asset.IsAudio()) addChip(asset.audioFormat, kAudioFormatChipColor);
    if (asset.HasAseprite()) addChip("ASE", kAsepriteChipColor);

    if (context_.projects.HasActive()) {
        const int inProject = static_cast<int>(std::count_if(keys.begin(), keys.end(), [&](const std::string& key) {
            return context_.projects.ActiveContains(key);
        }));
        if (inProject == static_cast<int>(keys.size())) {
            addChip("IN", kInProjectChipColor);
        } else if (inProject > 0) {
            addChip("IN " + std::to_string(inProject) + "/" + std::to_string(keys.size()), kInProjectChipColor);
        }
    }

    if (tile.isCollapsedStack) {
        const std::string countText = "x" + std::to_string(tile.variationIndices.size());
        const ImVec2 textSize = ImGui::CalcTextSize(countText.c_str());
        const ImVec2 badgeMin(areaMin.x + areaSize - textSize.x - 11.0f, areaMin.y + 3.0f);
        drawList->AddRectFilled(badgeMin, ImVec2(badgeMin.x + textSize.x + 8.0f, badgeMin.y + lineHeight + 2.0f), kVariationColor, 3.0f);
        drawList->AddText(ImVec2(badgeMin.x + 4.0f, badgeMin.y + 1.0f), IM_COL32(255, 255, 255, 255), countText.c_str());
    }
}

void LibraryView::RenderTileTooltip(const library::Asset& asset) const {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(asset.name.c_str());
    if (!asset.folder.empty()) ImGui::TextDisabled("%s", asset.folder.c_str());
    if (asset.IsAudio()) ImGui::Text("Audio: %s", asset.audioPath.filename().string().c_str());
    if (asset.HasBaseImage()) ImGui::Text("Base: %s", asset.baseImagePath.filename().string().c_str());
    if (asset.HasAseprite()) {
        ImGui::Text("Aseprite: %s", asset.asepritePath.filename().string().c_str());
    } else if (!asset.IsAudio()) {
        ImGui::TextDisabled("Not converted yet");
    }
    ImGui::EndTooltip();
}

void LibraryView::RenderTileMenu(const library::GridTile& tile, const std::set<std::string>& keys) {
    if (!ImGui::BeginPopupContextItem("##tileMenu")) return;
    const library::Asset& asset = context_.assets[tile.assetIndex];

    if (tile.IsInGroup()) {
        if (tile.isCollapsedStack) {
            const std::string label = "Show variations (" + std::to_string(tile.variationIndices.size()) + ")";
            if (ImGui::MenuItem(label.c_str())) expandedGroups_.insert(tile.groupKey);
        } else if (ImGui::MenuItem("Hide variations")) {
            expandedGroups_.erase(tile.groupKey);
        }
        ImGui::Separator();
    }

    if (asset.IsAudio()) {
        if (ImGui::MenuItem("Open in Audacity")) ShowToolResult(services::OpenInAudacity(asset.audioPath, context_.settings));
    } else {
        if (ImGui::MenuItem("Convert...", nullptr, false, asset.HasBaseImage())) converter_.OpenAsset(asset);
        if (ImGui::MenuItem("Open in Aseprite", nullptr, false, asset.HasAseprite())) ShowToolResult(services::OpenInAseprite(asset.asepritePath));
    }
    ImGui::Separator();

    if (!context_.projects.HasActive()) {
        ImGui::TextDisabled("Select or create a project first");
    } else {
        const bool allInProject = std::all_of(keys.begin(), keys.end(), [&](const std::string& key) {
            return context_.projects.ActiveContains(key);
        });
        const std::string suffix = tile.isCollapsedStack ? " (all " + std::to_string(keys.size()) + " variations)" : "";
        if (allInProject) {
            if (ImGui::MenuItem(("Remove from project" + suffix).c_str())) context_.projects.RemoveFromActive(keys);
        } else if (ImGui::MenuItem(("Add to project" + suffix).c_str())) {
            context_.projects.AddToActive(keys);
        }
    }
    ImGui::EndPopup();
}

// Click previews, Ctrl+click toggles selection, double-click opens or closes a variation group.
void LibraryView::HandleTileClicks(const library::GridTile& tile, const std::set<std::string>& keys, bool clicked, bool hovered,
                                   bool selected) {
    if (hovered && tile.IsInGroup() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (tile.isCollapsedStack) {
            expandedGroups_.insert(tile.groupKey);
        } else {
            expandedGroups_.erase(tile.groupKey);
        }
    }
    if (!clicked) return;

    if (ImGui::GetIO().KeyCtrl) {
        for (const std::string& key : keys) {
            if (selected) {
                context_.selectedKeys.erase(key);
            } else {
                context_.selectedKeys.insert(key);
            }
        }
    } else {
        preview_.Open(context_.assets[tile.assetIndex]);
    }
}

std::set<std::string> LibraryView::KeysOf(const library::GridTile& tile) const {
    std::set<std::string> keys;
    if (tile.isCollapsedStack) {
        for (const int index : tile.variationIndices) keys.insert(context_.assets[index].key);
    } else {
        keys.insert(context_.assets[tile.assetIndex].key);
    }
    return keys;
}

// ---- Actions ----

void LibraryView::PickLibraryFolder() {
    NFD::UniquePathN chosen;
    const auto& currentRoot = context_.settings.libraryRoot;
    const nfdresult_t result = NFD::PickFolder(chosen, currentRoot.empty() ? nullptr : currentRoot.c_str());
    if (result == NFD_OKAY) {
        context_.OpenLibrary(std::filesystem::path(chosen.get()));
    } else if (result == NFD_ERROR) {
        context_.statusMessage = std::string("File dialog error: ") + NFD::GetError();
    }
}

// Opens every selected asset that has a base image as one batch in the converter.
void LibraryView::ConvertSelected() {
    std::vector<std::filesystem::path> baseImages;
    size_t withoutBase = 0;
    for (const library::Asset& asset : context_.assets) {
        if (!context_.selectedKeys.count(asset.key)) continue;
        if (asset.HasBaseImage()) {
            baseImages.push_back(asset.baseImagePath);
        } else {
            ++withoutBase;
        }
    }
    if (baseImages.empty()) {
        context_.statusMessage = "None of the selected assets has a PNG/JPG base to convert";
        return;
    }
    converter_.OpenBatch(std::move(baseImages), withoutBase);
}

void LibraryView::ExportSpriteSheet() {
    if (!context_.projects.HasActive()) return;
    const std::string status = services::ExportSpriteSheet(context_.projects.Active(), context_.assets, context_.settings.libraryRoot);
    if (!status.empty()) context_.statusMessage = status;
}

void LibraryView::ShowToolResult(const std::string& error) {
    if (!error.empty()) context_.statusMessage = error;
}

} // namespace pixie::views
