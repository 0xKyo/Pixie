#pragma once

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "app/services/ConversionPipeline.h"

namespace pixie::services {

// How Convert All names each output file.
enum class OutputNaming {
    BesideOriginal, // <folder of source>/<source name>.aseprite
    KeepName,       // <output folder>/<source name>.<ext>
    Numbered,       // <output folder>/<base name>_01.<ext>, _02, ...
};

// Everything a batch needs, copied from the UI so the worker thread never touches UI state.
struct BatchJob {
    std::vector<std::filesystem::path> sources;
    ConversionSettings settings;
    color::Palette palette; // empty = keep the original colors

    bool cropToTargetAspect = false;
    graphics::CropRect previewCrop;      // the user's sampling box, reused for images the size of the preview
    int previewWidth = 0;
    int previewHeight = 0;

    size_t previewIndex = 0;                     // which source is the one shown in the converter
    std::optional<graphics::Mask> previewMask;   // its hand-edited background mask, if any
    bool removeBackground = false;               // other images get an automatic mask
    double backgroundTolerance = 10.0;

    OutputNaming naming = OutputNaming::Numbered;
    std::filesystem::path outputFolder;
    std::string baseName = "converted"; // UTF-8, for OutputNaming::Numbered
    bool saveAsAseprite = false;
};

// Written by the worker; the UI may read `processed` at any time and the rest once it finishes.
struct BatchProgress {
    std::atomic<int> processed{0};
    int saved = 0;
    int failed = 0;
    std::string firstError;

    void Reset();
};

// Runs the job's pipeline on one already-loaded source (its crop, background mask, palette and resize).
graphics::Image ConvertBatchSource(const BatchJob& job, size_t sourceIndex, const graphics::Image& source);

// Converts and saves every source in order (meant to run on a background thread).
void RunBatch(const BatchJob& job, BatchProgress& progress);

} // namespace pixie::services
