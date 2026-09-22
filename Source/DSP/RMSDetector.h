#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // RMS: true power estimation.
    //
    //     power         = x^2
    //     smoothedPower = onePole(power, averagingTime)
    //     rms           = sqrt(smoothedPower)
    //
    // Reported in dB relative to a full-scale sine (AES17 convention: +3.01 dB),
    // so a steady sine reads the same on PEAK and RMS and the COMPRESS macro
    // stays calibrated when switching detectors. Transient material reads lower
    // on RMS than on PEAK, which is exactly the "less transient sensitive"
    // behaviour wanted.
    class RMSDetector
    {
    public:
        static constexpr float defaultAveragingMs = 12.0f;

        void prepare (double sampleRate, float averagingMs = defaultAveragingMs) noexcept
        {
            coeff = timeConstantCoeff (averagingMs, sampleRate);
        }

        void reset() noexcept { power = 0.0f; }

        float processPower (float x) noexcept
        {
            power = x * x + coeff * (power - x * x);
            return power;
        }

        float processDb (float x) noexcept
        {
            return powerToDb (2.0f * processPower (x));
        }

        float getPower() const noexcept { return power; }

    private:
        float coeff = 0.0f;
        float power = 0.0f;
    };
}
