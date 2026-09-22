#pragma once

#include "Nonlinear/DCBlocker.h"
#include "Nonlinear/SaturationUtilities.h"

namespace heat::dsp
{
    // The compressor's own amplifier character, applied right after the gain
    // element (runs oversampled). CLEAN has none (drive 0 → exact identity).
    // WARM adds gentle, slightly asymmetric rounding that catches the
    // transients a softer attack lets through; DRIVE pushes harder and its
    // drive rises further as gain reduction deepens (compression and
    // saturation interact).
    class ModeColor
    {
    public:
        void prepare (double oversampledRate) noexcept
        {
            dc.prepare (oversampledRate);
        }

        void reset() noexcept { dc.reset(); }

        // drive: 0..~1.2 (mode colour + GR interaction), bias: asymmetry.
        void set (float driveAmount, float biasAmount) noexcept
        {
            active = driveAmount > 1.0e-4f;
            depth = std::min (1.0f, driveAmount / 0.05f);
            gain = 0.25f + 3.0f * driveAmount;
            bias = biasAmount * std::min (1.0f, driveAmount * 2.0f);
            const float slope = sat::algebraicSlope (bias, 1.0f);
            norm = 1.0f / (gain * slope);
            offset = sat::algebraic (bias, 1.0f);
        }

        float process (float x) noexcept
        {
            if (! active)
                return x + dc.process (0.0f);
            const float shaped = (sat::algebraic (gain * x + bias, 1.0f) - offset) * norm;
            return x + dc.process (depth * (shaped - x));
        }

    private:
        DCBlocker dc;
        bool active = false;
        float depth = 0.0f, gain = 1.0f, bias = 0.0f, norm = 1.0f, offset = 0.0f;
    };
}
