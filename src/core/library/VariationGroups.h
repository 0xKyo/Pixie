#pragma once

#include <set>
#include <string>
#include <vector>

#include "core/library/Asset.h"

namespace pixie::library {

// One tile of the library grid: a single asset or a collapsed stack of variations.
struct GridTile {
    int assetIndex = 0;                // the asset drawn (the first variation, for a stack)
    std::vector<int> variationIndices; // collapsed stacks only: every variation in the stack
    std::string groupKey;              // empty unless the asset belongs to a variation group
    bool isCollapsedStack = false;
    std::string stackLabel;            // collapsed stacks only, e.g. "Armor x5"

    [[nodiscard]] bool IsInGroup() const { return !groupKey.empty(); }
};

// Group key shared by same-kind, same-folder assets named alike up to the first digit ("" if none).
std::string VariationGroupKey(const Asset& asset, std::string* displayPrefix = nullptr);

// Turns visible assets into tiles, collapsing groups of 2+ variations unless listed in `expandedGroups`.
std::vector<GridTile> BuildGridTiles(const std::vector<Asset>& assets, const std::vector<int>& visibleIndices,
                                     const std::set<std::string>& expandedGroups);

} // namespace pixie::library
