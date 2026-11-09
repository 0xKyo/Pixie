#include "core/library/Asset.h"

#include <algorithm>
#include <map>

#include "core/formats/Aseprite.h"

namespace pixie::library {

namespace {

std::string ToUtf8(const std::u8string& text) { return std::string(reinterpret_cast<const char*>(text.data()), text.size()); }

std::string ToLower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return text;
}

std::string ToUpper(std::string text) {
    for (char& c : text) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    }
    return text;
}

std::string LowercaseExtension(const std::filesystem::path& path) { return ToLower(ToUtf8(path.extension().u8string())); }

std::string MakeKey(const std::string& folder, const std::string& name, const std::string& audioExtension) {
    return ToLower(folder) + "/" + ToLower(name) + audioExtension;
}

} // namespace

const char* KindLabel(AssetKind kind) {
    switch (kind) {
    case AssetKind::Image: return "Images";
    case AssetKind::Audio: return "Audio";
    default: return "?";
    }
}

const std::filesystem::path& Asset::MainFile() const {
    if (IsAudio()) return audioPath;
    return HasBaseImage() ? baseImagePath : asepritePath;
}

bool IsBaseImageFile(const std::filesystem::path& path) {
    const std::string extension = LowercaseExtension(path);
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}

bool IsAsepriteFile(const std::filesystem::path& path) { return formats::IsAsepritePath(path); }

bool IsAudioFile(const std::filesystem::path& path) {
    static const char* const kAudioExtensions[] = {".wav",  ".mp3",  ".ogg", ".oga", ".flac", ".aif",
                                                   ".aiff", ".aifc", ".m4a", ".aac", ".opus", ".wma",
                                                   ".wv",   ".ape",  ".mp2", ".au",  ".amr",  ".caf"};
    const std::string extension = LowercaseExtension(path);
    return std::any_of(std::begin(kAudioExtensions), std::end(kAudioExtensions),
                       [&](const char* candidate) { return extension == candidate; });
}

std::filesystem::path AsepritePathFor(const std::filesystem::path& baseImagePath) {
    std::filesystem::path result = baseImagePath;
    result.replace_extension(".aseprite");
    return result;
}

std::string KeyAfterRename(const Asset& asset, const std::string& newName) {
    return MakeKey(asset.folder, newName, asset.IsAudio() ? "." + ToLower(asset.audioFormat) : std::string());
}

std::vector<Asset> ScanFolder(const std::filesystem::path& root) {
    std::map<std::string, Asset> assetsByKey;

    std::error_code error;
    std::filesystem::recursive_directory_iterator entry(root, std::filesystem::directory_options::skip_permission_denied, error);
    for (const std::filesystem::recursive_directory_iterator end; !error && entry != end; entry.increment(error)) {
        std::error_code entryError;
        if (!entry->is_regular_file(entryError)) continue;

        const std::filesystem::path& path = entry->path();
        const bool isBaseImage = IsBaseImageFile(path);
        const bool isAseprite = IsAsepriteFile(path);
        const bool isAudio = !isBaseImage && !isAseprite && IsAudioFile(path);
        if (!isBaseImage && !isAseprite && !isAudio) continue;

        std::filesystem::path relativeFolder = std::filesystem::relative(path.parent_path(), root, entryError);
        if (entryError || relativeFolder == ".") relativeFolder.clear();
        const std::string folder = ToUtf8(relativeFolder.generic_u8string());
        const std::string name = ToUtf8(path.stem().u8string());
        // Audio keys include the extension: hit.wav and hit.ogg are two different assets.
        const std::string key = MakeKey(folder, name, isAudio ? LowercaseExtension(path) : std::string());

        Asset& asset = assetsByKey[key];
        if (asset.key.empty()) {
            asset.name = name;
            asset.lowercaseName = ToLower(name);
            asset.folder = folder;
            asset.key = key;
        }

        if (isAudio) {
            asset.kind = AssetKind::Audio;
            asset.audioPath = path;
            asset.audioFormat = ToUpper(LowercaseExtension(path).substr(1));
        } else if (isBaseImage) {
            const bool isPng = LowercaseExtension(path) == ".png";
            if (!asset.HasBaseImage() || (isPng && asset.baseImageFormat != "PNG")) { // PNG wins over JPEG
                asset.baseImagePath = path;
                asset.baseImageFormat = isPng ? "PNG" : "JPG";
            }
        } else if (!asset.HasAseprite()) {
            asset.asepritePath = path;
        }
    }

    std::vector<Asset> assets;
    assets.reserve(assetsByKey.size());
    for (auto& [key, asset] : assetsByKey) {
        assets.push_back(std::move(asset));
    }
    std::sort(assets.begin(), assets.end(), [](const Asset& first, const Asset& second) {
        if (first.folder != second.folder) return first.folder < second.folder;
        return first.lowercaseName < second.lowercaseName;
    });
    return assets;
}

} // namespace pixie::library
