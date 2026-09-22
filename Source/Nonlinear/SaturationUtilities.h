#pragma once

#include <cmath>

namespace heat::dsp::sat
{
    // Algebraic sigmoid with adjustable ceiling L:  v / sqrt(1 + (v/L)^2)
    // Smooth (C-infinity), unity slope at 0, saturates at ±L, costs one sqrt.
    inline float algebraic (float v, float ceiling) noexcept
    {
        const float u = v / ceiling;
        return v / std::sqrt (1.0f + u * u);
    }

    // Derivative of algebraic() at v.
    inline float algebraicSlope (float v, float ceiling) noexcept
    {
        const float u = v / ceiling;
        const float d = 1.0f + u * u;
        return 1.0f / (d * std::sqrt (d));
    }

    // Asymmetric transfer: the positive half-wave saturates at +posCeiling,
    // the negative half-wave earlier at -negCeiling. Both halves have unity
    // slope and zero curvature at the origin, so the join is smooth.
    inline float asymmetric (float v, float posCeiling, float negCeiling) noexcept
    {
        return v >= 0.0f ? algebraic (v, posCeiling) : algebraic (v, negCeiling);
    }

    inline float asymmetricSlope (float v, float posCeiling, float negCeiling) noexcept
    {
        return v >= 0.0f ? algebraicSlope (v, posCeiling) : algebraicSlope (v, negCeiling);
    }

    // Soft cubic that stays bounded: x^3 / (1 + x^2)  (≈ x^3 small, ≈ x large).
    inline float softCubic (float x) noexcept
    {
        const float x2 = x * x;
        return x * x2 / (1.0f + x2);
    }
}
