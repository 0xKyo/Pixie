#pragma once

#include "core/Crop.h"

namespace pixie {

// Largest rect, centered within an srcW x srcH image, that exactly matches
// the targetW:targetH aspect ratio. Used by the Downsize tool's "Maintain
// Ratio" sampling box, since the source image isn't necessarily the same
// proportion as the target size.
CropRect ComputeAspectFitRect(int srcW, int srcH, int targetW, int targetH);

} // namespace pixie
