#include "core/ColorMath.h"

#include <cmath>
#include <numbers>

namespace pixie {

namespace {

constexpr double kDeg2Rad = std::numbers::pi / 180.0;
constexpr double kRad2Deg = 180.0 / std::numbers::pi;

// sRGB D65 reference white.
constexpr double kXn = 95.0489;
constexpr double kYn = 100.0;
constexpr double kZn = 108.8840;

double SrgbChannelToLinear(double c) {
    c /= 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

double LabF(double t) {
    constexpr double kEpsilon = 216.0 / 24389.0; // (6/29)^3
    constexpr double kKappa = 24389.0 / 27.0;    // (29/3)^3
    if (t > kEpsilon) {
        return std::cbrt(t);
    }
    return (kKappa * t + 16.0) / 116.0;
}

// Normalizes an angle in degrees to [0, 360).
double NormalizeDegrees(double deg) {
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0) {
        deg += 360.0;
    }
    return deg;
}

} // namespace

Lab RgbToLab(RGB8 color) {
    const double r = SrgbChannelToLinear(color.r);
    const double g = SrgbChannelToLinear(color.g);
    const double b = SrgbChannelToLinear(color.b);

    // Linear sRGB -> CIE XYZ (D65), scaled to [0, 100].
    const double x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) * 100.0;
    const double y = (0.2126729 * r + 0.7151522 * g + 0.0721750 * b) * 100.0;
    const double z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) * 100.0;

    const double fx = LabF(x / kXn);
    const double fy = LabF(y / kYn);
    const double fz = LabF(z / kZn);

    Lab lab;
    lab.L = 116.0 * fy - 16.0;
    lab.a = 500.0 * (fx - fy);
    lab.b = 200.0 * (fy - fz);
    return lab;
}

// Reference implementation of CIEDE2000, following Sharma, Wu & Dalal (2005),
// "The CIEDE2000 Color-Difference Formula: Implementation Notes, Supplementary
// Test Data, and Mathematical Observations".
double Ciede2000(const Lab& lab1, const Lab& lab2) {
    constexpr double kL = 1.0;
    constexpr double kC = 1.0;
    constexpr double kH = 1.0;

    const double L1 = lab1.L, a1 = lab1.a, b1 = lab1.b;
    const double L2 = lab2.L, a2 = lab2.a, b2 = lab2.b;

    const double c1 = std::sqrt(a1 * a1 + b1 * b1);
    const double c2 = std::sqrt(a2 * a2 + b2 * b2);
    const double cBar = (c1 + c2) / 2.0;

    const double cBar7 = std::pow(cBar, 7.0);
    const double g = 0.5 * (1.0 - std::sqrt(cBar7 / (cBar7 + std::pow(25.0, 7.0))));

    const double a1Prime = a1 * (1.0 + g);
    const double a2Prime = a2 * (1.0 + g);

    const double c1Prime = std::sqrt(a1Prime * a1Prime + b1 * b1);
    const double c2Prime = std::sqrt(a2Prime * a2Prime + b2 * b2);

    auto hueAngle = [](double aPrime, double b) -> double {
        if (aPrime == 0.0 && b == 0.0) {
            return 0.0;
        }
        return NormalizeDegrees(std::atan2(b, aPrime) * kRad2Deg);
    };

    const double h1Prime = hueAngle(a1Prime, b1);
    const double h2Prime = hueAngle(a2Prime, b2);

    const double deltaLPrime = L2 - L1;
    const double deltaCPrime = c2Prime - c1Prime;

    double deltaHueDeg = 0.0;
    if (c1Prime * c2Prime != 0.0) {
        deltaHueDeg = h2Prime - h1Prime;
        if (deltaHueDeg > 180.0) {
            deltaHueDeg -= 360.0;
        } else if (deltaHueDeg < -180.0) {
            deltaHueDeg += 360.0;
        }
    }
    const double deltaHPrime = 2.0 * std::sqrt(c1Prime * c2Prime) * std::sin((deltaHueDeg * kDeg2Rad) / 2.0);

    const double lBarPrime = (L1 + L2) / 2.0;
    const double cBarPrime = (c1Prime + c2Prime) / 2.0;

    double hBarPrime;
    if (c1Prime * c2Prime == 0.0) {
        hBarPrime = h1Prime + h2Prime;
    } else if (std::abs(h1Prime - h2Prime) <= 180.0) {
        hBarPrime = (h1Prime + h2Prime) / 2.0;
    } else if (h1Prime + h2Prime < 360.0) {
        hBarPrime = (h1Prime + h2Prime + 360.0) / 2.0;
    } else {
        hBarPrime = (h1Prime + h2Prime - 360.0) / 2.0;
    }

    const double t = 1.0
        - 0.17 * std::cos((hBarPrime - 30.0) * kDeg2Rad)
        + 0.24 * std::cos((2.0 * hBarPrime) * kDeg2Rad)
        + 0.32 * std::cos((3.0 * hBarPrime + 6.0) * kDeg2Rad)
        - 0.20 * std::cos((4.0 * hBarPrime - 63.0) * kDeg2Rad);

    const double deltaTheta = 30.0 * std::exp(-std::pow((hBarPrime - 275.0) / 25.0, 2.0));

    const double cBarPrime7 = std::pow(cBarPrime, 7.0);
    const double rc = 2.0 * std::sqrt(cBarPrime7 / (cBarPrime7 + std::pow(25.0, 7.0)));

    const double lBarMinus50Sq = (lBarPrime - 50.0) * (lBarPrime - 50.0);
    const double sl = 1.0 + (0.015 * lBarMinus50Sq) / std::sqrt(20.0 + lBarMinus50Sq);
    const double sc = 1.0 + 0.045 * cBarPrime;
    const double sh = 1.0 + 0.015 * cBarPrime * t;

    const double rt = -std::sin(2.0 * deltaTheta * kDeg2Rad) * rc;

    const double termL = deltaLPrime / (kL * sl);
    const double termC = deltaCPrime / (kC * sc);
    const double termH = deltaHPrime / (kH * sh);

    const double deltaE = std::sqrt(termL * termL + termC * termC + termH * termH + rt * termC * termH);
    return deltaE;
}

} // namespace pixie
