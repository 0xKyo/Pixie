#include "app/AppContext.h"

#include "core/audio/AudioClip.h"
#include "core/audio/Normalize.h"
#include "core/audio/OggWrite.h"
#include "core/audio/Resample.h"
#include "core/audio/WavWrite.h"

namespace pixie::app {

namespace {

// A file that now lives under another name, so projects must follow it.
struct RenamedAudio {
    std::string oldKey;
    std::filesystem::path newPath;
};

struct AudioImportResult {
    std::vector<RenamedAudio> renamed;
    size_t musicToOgg = 0;
    size_t sfxToWav = 0;     // other formats rewritten as .wav (original deleted)
    size_t rewritten = 0;    // .wav/.ogg files fixed in place (format and/or loudness)
    std::string failures;

    [[nodiscard]] size_t TouchedCount() const { return musicToOgg + sfxToWav + rewritten; }
};

// SFX: 16-bit stereo 48 kHz with the peak at kSfxPeakDb.
audio::AudioClip ToSfxClip(const audio::AudioClip& clip) {
    audio::AudioClip result = audio::ConvertToStandardFormat(clip);
    audio::NormalizePeak(result, audio::kSfxPeakDb);
    return result;
}

bool SaveMusic(const audio::AudioClip& clip, const std::filesystem::path& path, std::string& error) {
    return audio::SaveOgg(audio::ConvertToStandardFormat(clip), path, audio::kMusicOggQuality, &error);
}

// Writes to a temp file first so a failed write never damages the original.
template <typename Writer>
bool ReplaceFileInPlace(const std::filesystem::path& path, Writer write, std::string& error) {
    std::filesystem::path temp = path;
    temp += ".pixie-tmp";
    std::error_code fileError;
    if (!write(temp)) {
        std::filesystem::remove(temp, fileError);
        return false;
    }
    std::filesystem::rename(temp, path, fileError);
    if (fileError) {
        error = fileError.message();
        std::filesystem::remove(temp, fileError);
        return false;
    }
    return true;
}

// Brings every audio file to its category's format. Music is stereo 48 kHz Ogg Vorbis (quality 0.5); SFX is 16-bit
// stereo 48 kHz WAV with a normalized peak. Files in another format are written beside the original, which is then
// deleted; .wav/.ogg files that only need fixing are rewritten in place.
AudioImportResult ImportAudio(const std::vector<library::Asset>& assets) {
    AudioImportResult result;
    for (const library::Asset& asset : assets) {
        if (!asset.IsAudio() || !audio::CanDecode(asset.audioPath)) continue;
        const bool isMusic = asset.IsMusic();
        const char* targetFormat = isMusic ? "OGG" : "WAV";

        const auto addFailure = [&](const std::string& error) {
            result.failures += (result.failures.empty() ? "" : "; ") + asset.audioPath.filename().string() + ": " + error;
        };
        std::string error;
        audio::AudioClip clip;

        if (asset.audioFormat == targetFormat) {
            if (isMusic) {
                if (audio::HasOggFormat(asset.audioPath, audio::kStandardChannels, audio::kStandardSampleRate)) continue;
                if (!audio::LoadClip(asset.audioPath, clip, &error) ||
                    !ReplaceFileInPlace(asset.audioPath, [&](const std::filesystem::path& temp) { return SaveMusic(clip, temp, error); }, error)) {
                    addFailure(error);
                    continue;
                }
            } else {
                const bool formatOk = audio::HasWavFormat(asset.audioPath, audio::kStandardChannels, audio::kStandardSampleRate,
                                                          audio::kSfxBitsPerSample);
                if (!audio::LoadClip(asset.audioPath, clip, &error)) {
                    addFailure(error);
                    continue;
                }
                if (formatOk && audio::IsPeakNormalized(clip, audio::kSfxPeakDb)) continue;
                const audio::AudioClip fixed = ToSfxClip(clip);
                if (!ReplaceFileInPlace(asset.audioPath, [&](const std::filesystem::path& temp) { return audio::SaveWav(fixed, temp, &error); }, error)) {
                    addFailure(error);
                    continue;
                }
            }
            ++result.rewritten;
            continue;
        }

        std::filesystem::path newPath = asset.audioPath;
        newPath.replace_extension(isMusic ? ".ogg" : ".wav");
        std::error_code existsError;
        if (std::filesystem::exists(newPath, existsError)) {
            addFailure(newPath.filename().string() + " already exists, left unconverted"); // never replace a file with a converted copy
            continue;
        }
        const bool written = audio::LoadClip(asset.audioPath, clip, &error) &&
                             (isMusic ? SaveMusic(clip, newPath, error) : audio::SaveWav(ToSfxClip(clip), newPath, &error));
        if (!written) {
            addFailure(error);
            continue; // keep the original if the new file could not be written
        }
        std::error_code removeError;
        std::filesystem::remove(asset.audioPath, removeError);
        result.renamed.push_back({asset.key, newPath});
        ++(isMusic ? result.musicToOgg : result.sfxToWav);
    }
    return result;
}

} // namespace

size_t AppContext::RescanLibrary() {
    if (settings.libraryRoot.empty()) {
        assets.clear();
        return 0;
    }
    assets = library::ScanFolder(settings.libraryRoot);

    const AudioImportResult imported = ImportAudio(assets);
    if (imported.TouchedCount() > 0) {
        assets = library::ScanFolder(settings.libraryRoot);
        for (const RenamedAudio& renamed : imported.renamed) {
            for (const library::Asset& asset : assets) {
                if (asset.IsAudio() && asset.audioPath == renamed.newPath) projects.RenameAssetKey(renamed.oldKey, asset.key); // projects follow the file
            }
        }
        statusMessage = "Audio imported: " + std::to_string(imported.musicToOgg) + " music to OGG, " + std::to_string(imported.sfxToWav) +
                        " SFX to WAV, " + std::to_string(imported.rewritten) + " fixed in place (stereo 48 kHz)";
    }
    if (!imported.failures.empty()) statusMessage = "Audio conversion failed: " + imported.failures;

    std::set<std::string> existingKeys;
    for (const library::Asset& asset : assets) existingKeys.insert(asset.key);
    for (auto key = selectedKeys.begin(); key != selectedKeys.end();) {
        key = existingKeys.count(*key) ? std::next(key) : selectedKeys.erase(key);
    }
    return imported.TouchedCount();
}

void AppContext::OpenLibrary(const std::filesystem::path& root) {
    settings.libraryRoot = root;
    settings.Save();
    thumbnails.Clear();
    selectedKeys.clear();
    projects.Load(root);
    RescanLibrary();
    screen = Screen::Library;
    statusMessage = "Library: " + root.string() + " (" + std::to_string(assets.size()) + " assets)";
}

bool AppContext::OpenSavedLibrary() {
    std::error_code error;
    if (settings.libraryRoot.empty() || !std::filesystem::is_directory(settings.libraryRoot, error)) return false;
    projects.Load(settings.libraryRoot);
    RescanLibrary();
    screen = Screen::Library;
    return true;
}

} // namespace pixie::app
