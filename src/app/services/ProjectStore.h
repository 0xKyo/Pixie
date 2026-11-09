#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace pixie::services {

// A named subset of the library, e.g. the assets one game uses.
struct Project {
    std::string name;
    std::set<std::string> assetKeys;
};

// The library's projects (stored in <library>/pixie-projects.txt) and which one is selected.
class ProjectStore {
public:
    // Replaces the projects with those saved under `libraryRoot` and selects none.
    void Load(const std::filesystem::path& libraryRoot);
    void Save() const;

    [[nodiscard]] const std::vector<Project>& All() const { return projects_; }
    [[nodiscard]] bool HasActive() const { return activeIndex_ >= 0 && activeIndex_ < static_cast<int>(projects_.size()); }
    [[nodiscard]] const Project& Active() const { return projects_[activeIndex_]; }
    [[nodiscard]] int ActiveIndex() const { return activeIndex_; }
    [[nodiscard]] bool ActiveContains(const std::string& assetKey) const;

    void SelectProject(int index); // -1 = whole library
    void CreateAndSelect(const std::string& name);
    void DeleteActive();
    void AddToActive(const std::set<std::string>& assetKeys);
    void RemoveFromActive(const std::set<std::string>& assetKeys);
    // Keeps memberships after an asset is renamed.
    void RenameAssetKey(const std::string& oldKey, const std::string& newKey);

private:
    std::filesystem::path file_;
    std::vector<Project> projects_;
    int activeIndex_ = -1;
};

} // namespace pixie::services
