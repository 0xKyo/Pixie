#include "app/services/BatchConversion.h"

#include <algorithm>
#include <set>

#include "app/util/Text.h"
#include "core/formats/ImageFile.h"
#include "core/library/Asset.h"

namespace pixie::services {

namespace {

// The user's box for images the size of the previewed one, otherwise a centered box of the right aspect.
graphics::CropRect ChooseCrop(const BatchJob& job, const graphics::Image& image) {
    if (!job.cropToTargetAspect) return {};
    const bool sameSizeAsPreview = image.Width() == job.previewWidth && image.Height() == job.previewHeight;
    if (sameSizeAsPreview && !job.previewCrop.Empty()) return job.previewCrop;
    return graphics::CenteredRectWithAspect(image.Width(), image.Height(), job.settings.targetWidth, job.settings.targetHeight);
}

// Output path without extension; `usedNames` keeps KeepName outputs from overwriting each other.
std::filesystem::path OutputPathWithoutExtension(const BatchJob& job, size_t sourceIndex,
                                                 std::set<std::filesystem::path::string_type>& usedNames) {
    const std::filesystem::path& source = job.sources[sourceIndex];
    switch (job.naming) {
    case OutputNaming::BesideOriginal:
        return source.parent_path() / source.stem();

    case OutputNaming::KeepName: {
        std::filesystem::path name = source.stem();
        for (int suffix = 2; !usedNames.insert(name.native()).second; ++suffix) {
            name = source.stem();
            name += "_" + std::to_string(suffix);
        }
        return job.outputFolder / name;
    }

    case OutputNaming::Numbered:
    default: {
        // Zero-padded to at least two digits, wider if there are 100+ images.
        const size_t digits = std::max<size_t>(2, std::to_string(job.sources.size()).size());
        std::string number = std::to_string(sourceIndex + 1);
        number.insert(0, digits - std::min(digits, number.size()), '0');
        std::filesystem::path path = job.outputFolder / util::Utf8Path(job.baseName);
        path += "_" + number;
        return path;
    }
    }
}

} // namespace

void BatchProgress::Reset() {
    processed = 0;
    saved = 0;
    failed = 0;
    firstError.clear();
}

graphics::Image ConvertBatchSource(const BatchJob& job, size_t sourceIndex, const graphics::Image& source) {
    graphics::Mask automaticMask;
    const graphics::Mask* mask = nullptr;
    if (sourceIndex == job.previewIndex && job.previewMask) {
        mask = &*job.previewMask;
    } else if (job.removeBackground) {
        graphics::DetectBackground(source, job.backgroundTolerance, automaticMask);
        mask = &automaticMask;
    }
    const color::Palette* palette = job.palette.Empty() ? nullptr : &job.palette;
    return ConvertImage(source, job.settings, mask, ChooseCrop(job, source), palette);
}

void RunBatch(const BatchJob& job, BatchProgress& progress) {
    std::set<std::filesystem::path::string_type> usedNames;

    for (size_t index = 0; index < job.sources.size(); ++index) {
        const std::filesystem::path& sourcePath = job.sources[index];
        std::string error;
        graphics::Image source;
        bool ok = formats::LoadImageFile(sourcePath, source, &error);
        if (ok && job.naming == OutputNaming::BesideOriginal && library::IsAsepriteFile(sourcePath)) {
            ok = false; // its output would overwrite the source itself
            error = "already an Aseprite file";
        }

        if (ok) {
            const graphics::Image result = ConvertBatchSource(job, index, source);

            std::filesystem::path outputPath = OutputPathWithoutExtension(job, index, usedNames);
            outputPath += job.saveAsAseprite ? ".aseprite" : ".png";
            ok = job.saveAsAseprite ? formats::SaveAseprite(result, outputPath, &error)
                                    : formats::SavePng(result, outputPath, &error);
        }

        if (ok) {
            ++progress.saved;
        } else {
            if (progress.failed == 0) progress.firstError = sourcePath.filename().string() + ": " + error;
            ++progress.failed;
        }
        ++progress.processed;
    }
}

} // namespace pixie::services
