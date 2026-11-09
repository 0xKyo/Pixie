#include "core/color/Color.h"

#include <cmath>
#include <numbers>

namespace pixie::color {

namespace {

constexpr double kDegreesToRadians = std::numbers::pi / 180.0;
constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi;

// D65 reference white, in XYZ scaled to [0, 100].
constexpr double kWhiteX = 95.0489;
constexpr double kWhiteY = 100.0;
constexpr double kWhiteZ = 108.8840;

// Undoes the sRGB gamma curve: 0..255 -> linear 0..1.
double SrgbToLinear(double channel) {
    channel /= 255.0;
    if (channel <= 0.04045) {
        return channel / 12.92;
    }
    return std::pow((channel + 0.055) / 1.055, 2.4);
}

// The nonlinear compression used by the XYZ -> L*a*b* conversion.
double LabCompress(double ratio) {
    constexpr double kEpsilon = 216.0 / 24389.0; // (6/29)^3
    constexpr double kKappa = 24389.0 / 27.0;    // (29/3)^3
    if (ratio > kEpsilon) {
        return std::cbrt(ratio);
    }
    return (kKappa * ratio + 16.0) / 116.0;
}

// Wraps an angle in degrees into [0, 360).
double WrapDegrees(double degrees) {
    degrees = std::fmod(degrees, 360.0);
    return degrees < 0.0 ? degrees + 360.0 : degrees;
}

} // namespace

Lab ToLab(Rgba color) {
    const double red = SrgbToLinear(color.r);
    const double green = SrgbToLinear(color.g);
    const double blue = SrgbToLinear(color.b);

    // Linear sRGB -> CIE XYZ (D65), scaled to [0, 100].
    const double x = (0.4124564 * red + 0.3575761 * green + 0.1804375 * blue) * 100.0;
    const double y = (0.2126729 * red + 0.7151522 * green + 0.0721750 * blue) * 100.0;
    const double z = (0.0193339 * red + 0.1191920 * green + 0.9503041 * blue) * 100.0;

    const double fx = LabCompress(x / kWhiteX);
    const double fy = LabCompress(y / kWhiteY);
    const double fz = LabCompress(z / kWhiteZ);

    return Lab{116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

// Follows Sharma, Wu & Dalal (2005); variable names mirror the paper's symbols.
double Ciede2000(const Lab& first, const Lab& second) {
    constexpr double kL = 1.0;
    constexpr double kC = 1.0;
    constexpr double kH = 1.0;

    const double L1 = first.L, a1 = first.a, b1 = first.b;
    const double L2 = second.L, a2 = second.a, b2 = second.b;

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
        return WrapDegrees(std::atan2(b, aPrime) * kRadiansToDegrees);
    };

    const double h1Prime = hueAngle(a1Prime, b1);
    const double h2Prime = hueAngle(a2Prime, b2);

    const double deltaLPrime = L2 - L1;
    const double deltaCPrime = c2Prime - c1Prime;

    double deltaHueDegrees = 0.0;
    if (c1Prime * c2Prime != 0.0) {
        deltaHueDegrees = h2Prime - h1Prime;
        if (deltaHueDegrees > 180.0) {
            deltaHueDegrees -= 360.0;
        } else if (deltaHueDegrees < -180.0) {
            deltaHueDegrees += 360.0;
        }
    }
    const double deltaHPrime = 2.0 * std::sqrt(c1Prime * c2Prime) * std::sin((deltaHueDegrees * kDegreesToRadians) / 2.0);

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
        - 0.17 * std::cos((hBarPrime - 30.0) * kDegreesToRadians)
        + 0.24 * std::cos((2.0 * hBarPrime) * kDegreesToRadians)
        + 0.32 * std::cos((3.0 * hBarPrime + 6.0) * kDegreesToRadians)
        - 0.20 * std::cos((4.0 * hBarPrime - 63.0) * kDegreesToRadians);

    const double deltaTheta = 30.0 * std::exp(-std::pow((hBarPrime - 275.0) / 25.0, 2.0));

    const double cBarPrime7 = std::pow(cBarPrime, 7.0);
    const double rc = 2.0 * std::sqrt(cBarPrime7 / (cBarPrime7 + std::pow(25.0, 7.0)));

    const double lBarMinus50Squared = (lBarPrime - 50.0) * (lBarPrime - 50.0);
    const double sl = 1.0 + (0.015 * lBarMinus50Squared) / std::sqrt(20.0 + lBarMinus50Squared);
    const double sc = 1.0 + 0.045 * cBarPrime;
    const double sh = 1.0 + 0.015 * cBarPrime * t;

    const double rt = -std::sin(2.0 * deltaTheta * kDegreesToRadians) * rc;

    const double termL = deltaLPrime / (kL * sl);
    const double termC = deltaCPrime / (kC * sc);
    const double termH = deltaHPrime / (kH * sh);

    return std::sqrt(termL * termL + termC * termC + termH * termH + rt * termC * termH);
}

} // namespace pixie::color
