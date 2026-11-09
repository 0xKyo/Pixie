#include "app/services/ProjectStore.h"

#include <fstream>

namespace pixie::services {

// File format: a "project=<name>" line followed by one "asset=<key>" line per member.
void ProjectStore::Load(const std::filesystem::path& libraryRoot) {
    projects_.clear();
    activeIndex_ = -1;
    file_ = libraryRoot / "pixie-projects.txt";

    std::ifstream input(file_, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("project=", 0) == 0) {
            projects_.push_back(Project{line.substr(8), {}});
        } else if (line.rfind("asset=", 0) == 0 && !projects_.empty()) {
            projects_.back().assetKeys.insert(line.substr(6));
        }
    }
}

void ProjectStore::Save() const {
    if (file_.empty()) return;
    std::ofstream output(file_, std::ios::binary | std::ios::trunc);
    for (const Project& project : projects_) {
        output << "project=" << project.name << "\n";
        for (const std::string& key : project.assetKeys) output << "asset=" << key << "\n";
    }
}

bool ProjectStore::ActiveContains(const std::string& assetKey) const {
    return HasActive() && projects_[activeIndex_].assetKeys.count(assetKey) > 0;
}

void ProjectStore::SelectProject(int index) {
    activeIndex_ = (index >= 0 && index < static_cast<int>(projects_.size())) ? index : -1;
}

void ProjectStore::CreateAndSelect(const std::string& name) {
    projects_.push_back(Project{name, {}});
    activeIndex_ = static_cast<int>(projects_.size()) - 1;
    Save();
}

void ProjectStore::DeleteActive() {
    if (!HasActive()) return;
    projects_.erase(projects_.begin() + activeIndex_);
    activeIndex_ = -1;
    Save();
}

void ProjectStore::AddToActive(const std::set<std::string>& assetKeys) {
    if (!HasActive()) return;
    projects_[activeIndex_].assetKeys.insert(assetKeys.begin(), assetKeys.end());
    Save();
}

void ProjectStore::RemoveFromActive(const std::set<std::string>& assetKeys) {
    if (!HasActive()) return;
    for (const std::string& key : assetKeys) projects_[activeIndex_].assetKeys.erase(key);
    Save();
}

void ProjectStore::RenameAssetKey(const std::string& oldKey, const std::string& newKey) {
    bool changed = false;
    for (Project& project : projects_) {
        if (project.assetKeys.erase(oldKey)) {
            project.assetKeys.insert(newKey);
            changed = true;
        }
    }
    if (changed) Save();
}

} // namespace pixie::services
