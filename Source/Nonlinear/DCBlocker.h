#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // First-order DC blocker (one-zero, one-pole high-pass), double-precision
    // state so it stays accurate at high oversampled rates.
    //
    // HEAT applies it only to the *distortion component* of each nonlinear
    // stage (y = x + dcBlock(f(x) - x)), so the linear part of the wet signal
    // keeps exactly the same phase as the dry path.
    class DCBlocker
    {
    public:
        void prepare (double sampleRate, double cutoffHz = 3.0) noexcept
        {
            r = std::exp (-2.0 * pi * cutoffHz / sampleRate);
            reset();
        }

        void reset() noexcept { x1 = y1 = 0.0; }

        float process (float x) noexcept
        {
            const double y = static_cast<double> (x) - x1 + r * y1;
            x1 = x;
            y1 = y;
            return static_cast<float> (y);
        }

    private:
        double r = 0.9999, x1 = 0.0, y1 = 0.0;
    };

    // Simple one-pole low-pass (used for HF smoothing inside colour stages).
    class OnePoleLowpass
    {
    public:
        void setCutoff (double hz, double sampleRate) noexcept
        {
            const double fc = std::min (hz, 0.45 * sampleRate);
            k = static_cast<float> (-std::expm1 (-2.0 * pi * fc / sampleRate));
        }

        void reset() noexcept { z = 0.0f; }

        float process (float x) noexcept
        {
            z += k * (x - z);
            return z;
        }

    private:
        float k = 1.0f, z = 0.0f;
    };
}
