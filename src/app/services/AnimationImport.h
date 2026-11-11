#pragma once

#include <filesystem>
#include <vector>

#include "app/services/BatchConversion.h"

namespace pixie::services {

// The PNG/JPEG files directly inside `folder`, sorted by name with numbers compared by value (frame2 before frame10).
[[nodiscard]] std::vector<std::filesystem::path> ListAnimationFrames(const std::filesystem::path& folder);

// A folder of frames converted with the batch settings and saved as one Aseprite file.
struct AnimationJob {
    BatchJob conversion; // sources are the frames in order; its naming and output fields are unused
    int frameDurationMs = 100;
    std::filesystem::path outputPath;
};

// Converts every frame and writes the animation (meant to run on a background thread).
// `progress.saved` ends as the frame count, or 0 with `failed` = 1 and `firstError` set if nothing was written.
void RunAnimationImport(const AnimationJob& job, BatchProgress& progress);

} // namespace pixie::services
