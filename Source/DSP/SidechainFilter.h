#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // 2nd-order Butterworth high-pass (12 dB/oct) for the detector path only.
    // Topology-preserving-transform state-variable filter: unconditionally
    // stable and click-free under cutoff modulation. The cutoff is smoothed
    // in the log-frequency domain and coefficients are refreshed every
    // `updateInterval` samples.
    class SidechainFilter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int updateInterval = 16;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // Snaps the cutoff without smoothing (used on prepare / state load).
        void setCutoffImmediate (float hz) noexcept;
        void setCutoff (float hz) noexcept;

        // Processes in place. numChannels <= maxChannels.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        float getCurrentCutoff() const noexcept;

        // Magnitude response of the current (target) design; used by tests.
        static double magnitudeAt (double hz, double cutoffHz, double sampleRate) noexcept;

    private:
        void updateCoefficients() noexcept;

        double fs = 48000.0;
        OnePoleSmoother logCutoff;
        float g = 0.0f, k = 1.41421356f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float ic1eq[maxChannels] {}, ic2eq[maxChannels] {};
        int counter = 0;
    };
}
