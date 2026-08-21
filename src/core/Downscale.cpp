#include "core/Downscale.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pixie {

namespace {

// A premultiplied-alpha RGBA sample, channels normalized to [0, 1]. Kept as
// doubles through the accumulation to avoid repeated 8-bit rounding.
struct Sample {
    double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
};

Sample ToSample(RGB8 c) {
    const double a = c.a / 255.0;
    return Sample{(c.r / 255.0) * a, (c.g / 255.0) * a, (c.b / 255.0) * a, a};
}

RGB8 FromSample(const Sample& s) {
    const double a = std::clamp(s.a, 0.0, 1.0);
    if (a <= 1e-6) {
        return RGB8{0, 0, 0, 0};
    }
    auto toByte = [](double v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    return RGB8{toByte(s.r / a), toByte(s.g / a), toByte(s.b / a), toByte(a)};
}

// Coverage-weighted box resize of one axis: `get(i)` reads input sample i,
// `put(o, s)` stores output o, weighted by each input's overlap with the output cell's footprint.
template <typename Get, typename Put>
void BoxResize1D(int inCount, int outCount, Get get, Put put) {
    const double scale = static_cast<double>(inCount) / static_cast<double>(outCount);
    for (int o = 0; o < outCount; ++o) {
        const double start = o * scale;
        const double end = (o + 1) * scale;
        const int i0 = std::clamp(static_cast<int>(std::floor(start)), 0, inCount - 1);
        const int i1 = std::clamp(static_cast<int>(std::ceil(end)) - 1, 0, inCount - 1);

        Sample acc;
        double totalWeight = 0.0;
        for (int i = i0; i <= i1; ++i) {
            const double left = std::max(start, static_cast<double>(i));
            const double right = std::min(end, static_cast<double>(i + 1));
            const double weight = right - left;
            if (weight <= 0.0) continue;
            const Sample in = get(i);
            acc.r += in.r * weight;
            acc.g += in.g * weight;
            acc.b += in.b * weight;
            acc.a += in.a * weight;
            totalWeight += weight;
        }
        if (totalWeight > 0.0) {
            const double invWeight = 1.0 / totalWeight;
            acc.r *= invWeight;
            acc.g *= invWeight;
            acc.b *= invWeight;
            acc.a *= invWeight;
        }
        put(o, acc);
    }
}

} // namespace

void DownscaleImage(const Image& src, int dstWidth, int dstHeight, Image& dst) {
    if (src.Empty() || dstWidth <= 0 || dstHeight <= 0) {
        return;
    }
    const int srcW = src.Width();
    const int srcH = src.Height();

    // Pass 1: resize horizontally (srcW -> dstWidth), one row at a time.
    std::vector<Sample> horizontal(static_cast<size_t>(dstWidth) * static_cast<size_t>(srcH));
    for (int y = 0; y < srcH; ++y) {
        BoxResize1D(
            srcW, dstWidth, [&](int x) { return ToSample(src.GetPixel(x, y)); },
            [&](int x, const Sample& s) { horizontal[static_cast<size_t>(y) * dstWidth + x] = s; });
    }

    // Pass 2: resize vertically (srcH -> dstHeight) on the intermediate buffer.
    dst.Resize(dstWidth, dstHeight);
    for (int x = 0; x < dstWidth; ++x) {
        BoxResize1D(
            srcH, dstHeight, [&](int y) { return horizontal[static_cast<size_t>(y) * dstWidth + x]; },
            [&](int y, const Sample& s) { dst.SetPixel(x, y, FromSample(s)); });
    }
}

} // namespace pixie
