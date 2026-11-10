#pragma once

#include <imgui.h>

#include <set>
#include <string>
#include <vector>

#include "app/AppContext.h"
#include "app/views/ConverterView.h"
#include "app/views/PreviewPanel.h"
#include "core/library/VariationGroups.h"

namespace pixie::views {

// The home screen: a filterable thumbnail grid of the library, projects, and the preview side panel.
class LibraryView {
public:
    LibraryView(app::AppContext& context, ConverterView& converter, PreviewPanel& preview)
        : context_(context), converter_(converter), preview_(preview) {}

    void LoadSettings(); // restores the thumbnail size
    void SaveSettings(); // remembers the thumbnail size

    void RenderFolderPicker(); // first-run screen asking for the assets folder
    void Render();

private:
    enum class ConversionFilter { All, Converted, NotConverted };
    enum class ProjectScope { OnlyProjectAssets, WholeLibrary };

    // Toolbar rows.
    void RenderToolbar();
    void RenderFilterBar();
    void RenderTypeFilter();
    void RenderToggleVariationsButton(const std::vector<library::GridTile>& tiles);
    void RenderProjectBar();
    void RenderNewProjectButton();
    void RenderDeleteProjectButton();

    // Grid.
    [[nodiscard]] std::vector<int> VisibleAssetIndices() const;
    void RenderGrid(const std::vector<library::GridTile>& tiles);
    void RenderTile(const library::GridTile& tile, ImVec2 topLeft);
    void DrawTileImage(const library::Asset& asset, ImVec2 areaMin, float areaSize);
    void DrawTileChips(const library::GridTile& tile, const std::set<std::string>& keys, ImVec2 areaMin, float areaSize);
    void RenderTileTooltip(const library::Asset& asset) const;
    void RenderTileMenu(const library::GridTile& tile, const std::set<std::string>& keys);
    void HandleTileClicks(const library::GridTile& tile, const std::set<std::string>& keys, bool clicked, bool hovered,
                          bool selected);
    [[nodiscard]] std::set<std::string> KeysOf(const library::GridTile& tile) const; // a stack stands for all its variations

    // Actions.
    void PickLibraryFolder();
    void ConvertSelected();
    void ExportProject();
    void ShowToolResult(const std::string& error);

    app::AppContext& context_;
    ConverterView& converter_;
    PreviewPanel& preview_;

    char searchText_[128] = "";
    ConversionFilter conversionFilter_ = ConversionFilter::All;
    bool kindVisible_[library::kAssetKindCount] = {true, true};
    float tileSize_ = 140.0f;
    const std::vector<library::GridTile>* shownTiles_ = nullptr; // tiles of the current frame, for Shift+click ranges
    int selectionAnchor_ = -1;                                    // asset index where a Shift range starts
    std::set<std::string> expandedGroups_; // variation groups shown one tile per variation
    ProjectScope projectScope_ = ProjectScope::OnlyProjectAssets;
    char newProjectName_[64] = "";
};

} // namespace pixie::views
