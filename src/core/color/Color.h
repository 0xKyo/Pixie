#pragma once

#include <cstdint>

namespace pixie::color {

// 8-bit sRGB color with straight (non-premultiplied) alpha.
struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

inline constexpr Rgba kTransparent{0, 0, 0, 0};

// CIE L*a*b* color (D65 white point): L is lightness, a/b are the color axes.
struct Lab {
    double L = 0.0;
    double a = 0.0;
    double b = 0.0;
};

// Converts an sRGB color to L*a*b*; alpha is ignored.
Lab ToLab(Rgba color);

// Perceptual distance between two colors using CIEDE2000; 0 means identical.
double Ciede2000(const Lab& first, const Lab& second);

} // namespace pixie::color
