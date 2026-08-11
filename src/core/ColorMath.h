#pragma once

#include <cstdint>

namespace pixie {

// 8-bit sRGB color with alpha.
struct RGB8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// CIE L*a*b* color (D65 white point).
struct Lab {
    double L = 0.0;
    double a = 0.0;
    double b = 0.0;
};

// Converts an 8-bit sRGB color to CIE L*a*b* (D65 white point). Alpha is ignored.
Lab RgbToLab(RGB8 color);

// Perceptual color difference between two Lab colors using the CIEDE2000 formula.
// Returns 0 for identical colors; larger values mean more perceptually different.
double Ciede2000(const Lab& lab1, const Lab& lab2);

} // namespace pixie
