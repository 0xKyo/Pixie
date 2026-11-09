#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pixie::library {

// Kinds of assets the library shows; to add one, insert it before Count and give it a label.
enum class AssetKind { Image, Audio, Count };
inline constexpr int kAssetKindCount = static_cast<int>(AssetKind::Count);

[[nodiscard]] const char* KindLabel(AssetKind kind);

// One logical asset: an image (base PNG/JPEG and/or its .aseprite conversion) or an audio file.
struct Asset {
    AssetKind kind = AssetKind::Image;
    std::string name;          // UTF-8 file name without extension, as shown in the UI
    std::string lowercaseName; // for case-insensitive search
    std::string folder;        // UTF-8 folder relative to the library root ("" = root)
    std::string key;           // stable id: lowercase folder + "/" + name (+ extension for audio)

    std::filesystem::path baseImagePath; // full-scale PNG/JPEG, empty if none
    std::string baseImageFormat;         // "PNG" or "JPG"
    std::filesystem::path asepritePath;  // converted version, empty if not converted yet
    std::filesystem::path audioPath;     // audio assets only
    std::string audioFormat;             // "WAV", "MP3", ...

    [[nodiscard]] bool IsAudio() const { return kind == AssetKind::Audio; }
    [[nodiscard]] bool HasBaseImage() const { return !baseImagePath.empty(); }
    [[nodiscard]] bool HasAseprite() const { return !asepritePath.empty(); }
    // The file that represents the asset: the audio file, else the base image, else the .aseprite.
    [[nodiscard]] const std::filesystem::path& MainFile() const;
};

[[nodiscard]] bool IsBaseImageFile(const std::filesystem::path& path);
[[nodiscard]] bool IsAsepriteFile(const std::filesystem::path& path);
[[nodiscard]] bool IsAudioFile(const std::filesystem::path& path);

// Where the converted version of a base image lives: same folder and name, .aseprite extension.
[[nodiscard]] std::filesystem::path AsepritePathFor(const std::filesystem::path& baseImagePath);

// The key an asset gets once renamed to `newName` (same folder and kind).
[[nodiscard]] std::string KeyAfterRename(const Asset& asset, const std::string& newName);

// Recursively finds every image/audio file under `root` and pairs hero.png with hero.aseprite.
std::vector<Asset> ScanFolder(const std::filesystem::path& root);

} // namespace pixie::library
