#include "core/library/VariationGroups.h"

#include <map>

namespace pixie::library {

std::string VariationGroupKey(const Asset& asset, std::string* displayPrefix) {
    const size_t firstDigit = asset.name.find_first_of("0123456789");
    if (firstDigit == std::string::npos || firstDigit == 0) return {};

    // "Arrow_01a" and "Arrow-01a" group with "Arrow01a".
    std::string prefix = asset.name.substr(0, firstDigit);
    while (!prefix.empty() && (prefix.back() == '_' || prefix.back() == '-' || prefix.back() == ' ')) prefix.pop_back();
    if (prefix.empty()) return {};
    if (displayPrefix) *displayPrefix = prefix;

    for (char& c : prefix) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return std::to_string(static_cast<int>(asset.kind)) + "|" + asset.folder + "/" + prefix;
}

std::vector<GridTile> BuildGridTiles(const std::vector<Asset>& assets, const std::vector<int>& visibleIndices,
                                     const std::set<std::string>& expandedGroups) {
    std::map<std::string, std::vector<int>> membersByGroup;
    for (const int index : visibleIndices) {
        const std::string groupKey = VariationGroupKey(assets[index]);
        if (!groupKey.empty()) membersByGroup[groupKey].push_back(index);
    }

    std::vector<GridTile> tiles;
    tiles.reserve(visibleIndices.size());
    std::set<std::string> groupsAlreadyPlaced;

    for (const int index : visibleIndices) {
        std::string prefix;
        const std::string groupKey = VariationGroupKey(assets[index], &prefix);
        const auto group = groupKey.empty() ? membersByGroup.end() : membersByGroup.find(groupKey);

        // A lone asset, or a "group" with just one visible member, is a plain tile.
        if (group == membersByGroup.end() || group->second.size() < 2) {
            GridTile tile;
            tile.assetIndex = index;
            tiles.push_back(std::move(tile));
            continue;
        }

        // The whole group goes where its first member would be; later members are skipped.
        if (!groupsAlreadyPlaced.insert(groupKey).second) continue;

        if (expandedGroups.count(groupKey)) {
            for (const int member : group->second) {
                GridTile tile;
                tile.assetIndex = member;
                tile.groupKey = groupKey;
                tiles.push_back(std::move(tile));
            }
        } else {
            GridTile stack;
            stack.assetIndex = index;
            stack.variationIndices = group->second;
            stack.groupKey = groupKey;
            stack.isCollapsedStack = true;
            stack.stackLabel = prefix + " x" + std::to_string(group->second.size());
            tiles.push_back(std::move(stack));
        }
    }
    return tiles;
}

} // namespace pixie::library
