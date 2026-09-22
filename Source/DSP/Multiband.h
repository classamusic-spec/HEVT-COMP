#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // Topology-preserving-transform state-variable filter (Zavalishin /
    // Simper), one channel. tick() returns the band-pass (v1) and low-pass
    // (v2) states; high-pass = v0 − k·v1 − v2.
    struct TptSvf
    {
        float g = 0.0f, k = 1.41421356f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
        float ic1eq = 0.0f, ic2eq = 0.0f;

        void design (double fc, double sampleRate, double kk) noexcept
        {
            const double f = std::clamp (fc, 1.0, 0.45 * sampleRate);
            g = static_cast<float> (std::tan (pi * f / sampleRate));
            k = static_cast<float> (kk);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() noexcept { ic1eq = ic2eq = 0.0f; }

        void tick (float v0, float& v1, float& v2) noexcept
        {
            const float v3 = v0 - ic2eq;
            v1 = a1 * ic1eq + a2 * v3;
            v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
        }

        float lowpass (float v0) noexcept  { float v1, v2; tick (v0, v1, v2); return v2; }
        float highpass (float v0) noexcept { float v1, v2; tick (v0, v1, v2); return v0 - k * v1 - v2; }
    };

    // Detector-only band split: 4th-order Linkwitz–Riley (two cascaded
    // Butterworth sections) at the LOW and HIGH crossovers. The split feeds
    // the band compressors; it never touches the audio.
    //
    //   2 bands: LOW = LP(f1), HIGH = HP(f1)
    //   3 bands: LOW = LP(f1), MID = LP(f2)·HP(f1), HIGH = HP(f2)·HP(f1)
    class BandSplitter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int updateInterval = 16;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        void setCrossovers (float lowHz, float highHz) noexcept;
        void setCrossoversImmediate (float lowHz, float highHz) noexcept;

        // out[band][channel]; band 1 (MID) is only written for three bands.
        void process (const float* const* in, int numChannels, int numSamples, bool threeBands,
                      float* const (&out)[3][maxChannels]) noexcept;

        float getLowHz() const noexcept  { return std::exp (logLow.getCurrent()); }
        float getHighHz() const noexcept { return std::exp (logHigh.getCurrent()); }

        // Magnitude of one detector band (0 LOW, 1 MID, 2 HIGH) for tests.
        static double bandMagnitudeAt (int band, double hz, double lowHz, double highHz, bool threeBands,
                                       double sampleRate) noexcept;

    private:
        void design() noexcept;

        double fs = 48000.0;
        OnePoleSmoother logLow, logHigh;
        TptSvf lpLow[2][maxChannels], hpLow[2][maxChannels], lpHigh[2][maxChannels], hpHigh[2][maxChannels];
        int counter = 0;
    };

    // Dynamic shelving stage that applies band gains to the audio: a low shelf
    // at f1 carries LOW relative to the reference band, a high shelf at f2
    // carries HIGH relative to it (Simper SVF shelves, Q = 0.7071: monotonic,
    // no bump at the corner, midpoint gain = √G). With equal band gains both
    // shelves are exactly the identity — neutral processing and parallel MIX
    // stay phase-true — and the audio is never split, so there is nothing to
    // re-sum. Coefficients are computed once per base-rate sample and
    // interpolated across the oversampled sub-samples.
    struct ShelfCoefficients
    {
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;

        static ShelfCoefficients lowShelf (float g0, float gainDb) noexcept;
        static ShelfCoefficients highShelf (float g0, float gainDb) noexcept;
        static ShelfCoefficients lerp (const ShelfCoefficients& a, const ShelfCoefficients& b, float t) noexcept;

        // Magnitude at hz for corner fc (analysis only).
        static double lowShelfMagnitude (double hz, double fc, double gainDb, double sampleRate) noexcept;
        static double highShelfMagnitude (double hz, double fc, double gainDb, double sampleRate) noexcept;
    };

    struct ShelfState
    {
        float ic1eq = 0.0f, ic2eq = 0.0f;

        void reset() noexcept { ic1eq = ic2eq = 0.0f; }

        float process (float v0, const ShelfCoefficients& c) noexcept
        {
            const float v3 = v0 - ic2eq;
            const float v1 = c.a1 * ic1eq + c.a2 * v3;
            const float v2 = ic2eq + c.a2 * ic1eq + c.a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
        }
    };
}
