#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // OPTICAL: original program-dependent "light cell" detector.
    //
    // This is not a model of any specific optical compressor circuit. It is an
    // envelope behaviour inspired by the general idea of a light source driving
    // a slow photo-resistive element:
    //
    //   1. Emission: signal power drives a light level that rises quickly and
    //      decays more slowly (asymmetric power follower).
    //   2. The cell's own response (attack that speeds up for large level jumps,
    //      two-stage release with memory) lives in Ballistics, configured by the
    //      detector blend in CompressorEngine.
    //
    // The level is reported in AES17 dB like the RMS detector.
    class OpticalDetector
    {
    public:
        static constexpr float riseMs = 1.5f;
        static constexpr float fallMs = 18.0f;

        void prepare (double sampleRate) noexcept
        {
            riseCoeff = timeConstantCoeff (riseMs, sampleRate);
            fallCoeff = timeConstantCoeff (fallMs, sampleRate);
        }

        void reset() noexcept { light = 0.0f; }

        float processDb (float x) noexcept
        {
            const float p = x * x;
            const float c = p > light ? riseCoeff : fallCoeff;
            light = p + c * (light - p);
            return powerToDb (2.0f * light);
        }

    private:
        float riseCoeff = 0.0f, fallCoeff = 0.0f;
        float light = 0.0f;
    };
}
