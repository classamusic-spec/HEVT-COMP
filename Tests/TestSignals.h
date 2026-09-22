#pragma once

#include "DSP/DspMath.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace heat::test
{
    inline std::vector<float> sine (double fs, double hz, double amplitude, int numSamples, double phase = 0.0)
    {
        std::vector<float> v (static_cast<size_t> (numSamples));
        for (int i = 0; i < numSamples; ++i)
            v[static_cast<size_t> (i)] = static_cast<float> (amplitude * std::sin (2.0 * dsp::pi * hz * i / fs + phase));
        return v;
    }

    // Deterministic white noise (xorshift), uniform in [-1, 1).
    class Noise
    {
    public:
        explicit Noise (uint32_t seed = 0x1234567u) : state (seed) {}

        float next() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float> (state) / 2147483648.0f - 1.0f;
        }

    private:
        uint32_t state;
    };

    inline double rmsOf (const float* x, int n)
    {
        double s = 0.0;
        for (int i = 0; i < n; ++i)
            s += static_cast<double> (x[i]) * x[i];
        return std::sqrt (s / std::max (1, n));
    }

    inline double peakOf (const float* x, int n)
    {
        double p = 0.0;
        for (int i = 0; i < n; ++i)
            p = std::max (p, std::abs (static_cast<double> (x[i])));
        return p;
    }

    inline double meanOf (const float* x, int n)
    {
        double s = 0.0;
        for (int i = 0; i < n; ++i)
            s += x[i];
        return s / std::max (1, n);
    }

    inline double toDb (double g) { return 20.0 * std::log10 (std::max (1.0e-12, g)); }

    // Goertzel-style single-bin amplitude estimate with a Hann window.
    inline double toneAmplitude (const float* x, int n, double fs, double hz)
    {
        double re = 0.0, im = 0.0, wsum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * dsp::pi * i / (n - 1));
            const double ph = 2.0 * dsp::pi * hz * i / fs;
            re += w * x[i] * std::cos (ph);
            im += w * x[i] * std::sin (ph);
            wsum += w;
        }
        return 2.0 * std::sqrt (re * re + im * im) / wsum;
    }

    // Blackman-Harris windowed DFT magnitude spectrum (dB re full-scale sine),
    // computed with a radix-2 FFT. n must be a power of two.
    std::vector<double> spectrumDb (const float* x, int n);

    // THD (ratio, not %) of a steady tone: sum of harmonics 2..maxHarmonic
    // (below Nyquist) relative to the fundamental. Uses exact-bin tones.
    double thd (const float* x, int n, double fs, double fundamentalHz, int maxHarmonic = 10);
}
