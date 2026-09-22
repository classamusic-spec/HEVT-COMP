#include "DSP/Multiband.h"

#include <complex>

namespace heat::dsp
{
    namespace
    {
        constexpr double butterworthK = 1.41421356237;
        constexpr float crossoverSmoothingMs = 40.0f;
        constexpr float shelfK = 1.41421356f; // 1/Q, Q = 0.7071

        std::complex<double> sOf (double hz, double fc, double sampleRate)
        {
            const double f = std::clamp (fc, 1.0, 0.45 * sampleRate);
            const double gg = std::tan (pi * f / sampleRate);
            const std::complex<double> z = std::polar (1.0, 2.0 * pi * hz / sampleRate);
            return (1.0 / gg) * (z - 1.0) / (z + 1.0);
        }
    }

    // --- BandSplitter ---------------------------------------------------------------

    void BandSplitter::prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        logLow.prepare (sampleRate, crossoverSmoothingMs);
        logHigh.prepare (sampleRate, crossoverSmoothingMs);
        setCrossoversImmediate (150.0f, 2500.0f);
        reset();
    }

    void BandSplitter::reset() noexcept
    {
        for (int s = 0; s < 2; ++s)
            for (int ch = 0; ch < maxChannels; ++ch)
                for (auto* f : { &lpLow[s][ch], &hpLow[s][ch], &lpHigh[s][ch], &hpHigh[s][ch] })
                    f->reset();
        counter = 0;
    }

    void BandSplitter::setCrossovers (float lowHz, float highHz) noexcept
    {
        logLow.setTarget (std::log (std::max (1.0f, lowHz)));
        logHigh.setTarget (std::log (std::max (1.0f, highHz)));
    }

    void BandSplitter::setCrossoversImmediate (float lowHz, float highHz) noexcept
    {
        logLow.reset (std::log (std::max (1.0f, lowHz)));
        logHigh.reset (std::log (std::max (1.0f, highHz)));
        design();
    }

    void BandSplitter::design() noexcept
    {
        const double lo = std::exp (logLow.getCurrent());
        const double hi = std::exp (logHigh.getCurrent());
        for (int s = 0; s < 2; ++s)
            for (int ch = 0; ch < maxChannels; ++ch)
            {
                lpLow[s][ch].design (lo, fs, butterworthK);
                hpLow[s][ch].design (lo, fs, butterworthK);
                lpHigh[s][ch].design (hi, fs, butterworthK);
                hpHigh[s][ch].design (hi, fs, butterworthK);
            }
    }

    void BandSplitter::process (const float* const* in, int numChannels, int numSamples, bool threeBands,
                                float* const (&out)[3][maxChannels]) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);
        for (int i = 0; i < numSamples; ++i)
        {
            if (counter == 0)
            {
                if (logLow.isSmoothing() || logHigh.isSmoothing())
                {
                    logLow.skip (updateInterval);
                    logHigh.skip (updateInterval);
                    design();
                }
                counter = updateInterval;
            }
            --counter;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x = in[ch][i];
                const float low = lpLow[1][ch].lowpass (lpLow[0][ch].lowpass (x));
                const float upper = hpLow[1][ch].highpass (hpLow[0][ch].highpass (x));
                out[0][ch][i] = low;
                if (threeBands)
                {
                    out[1][ch][i] = lpHigh[1][ch].lowpass (lpHigh[0][ch].lowpass (upper));
                    out[2][ch][i] = hpHigh[1][ch].highpass (hpHigh[0][ch].highpass (upper));
                }
                else
                {
                    out[2][ch][i] = upper;
                }
            }
        }
    }

    double BandSplitter::bandMagnitudeAt (int band, double hz, double lowHz, double highHz, bool threeBands,
                                          double sampleRate) noexcept
    {
        auto lp2 = [&] (double fc) { const auto s = sOf (hz, fc, sampleRate); return 1.0 / (s * s + butterworthK * s + 1.0); };
        auto hp2 = [&] (double fc) { const auto s = sOf (hz, fc, sampleRate); return (s * s) / (s * s + butterworthK * s + 1.0); };

        const auto lowLp = lp2 (lowHz) * lp2 (lowHz);
        const auto lowHp = hp2 (lowHz) * hp2 (lowHz);
        if (band == 0)
            return std::abs (lowLp);
        if (! threeBands)
            return band == 2 ? std::abs (lowHp) : 0.0;
        if (band == 1)
            return std::abs (lowHp * lp2 (highHz) * lp2 (highHz));
        return std::abs (lowHp * hp2 (highHz) * hp2 (highHz));
    }

    // --- Shelves ----------------------------------------------------------------------

    ShelfCoefficients ShelfCoefficients::lowShelf (float g0, float gainDb) noexcept
    {
        // At 0 dB: m1 = m2 = 0 and m0 = 1, so the output is exactly the input
        // while the states keep tracking it (ready for the next gain change).
        ShelfCoefficients c;
        const float a = std::pow (10.0f, gainDb / 40.0f);
        const float g = g0 / std::sqrt (a);
        c.a1 = 1.0f / (1.0f + g * (g + shelfK));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        c.m0 = 1.0f;
        c.m1 = shelfK * (a - 1.0f);
        c.m2 = a * a - 1.0f;
        return c;
    }

    ShelfCoefficients ShelfCoefficients::highShelf (float g0, float gainDb) noexcept
    {
        ShelfCoefficients c;
        const float a = std::pow (10.0f, gainDb / 40.0f);
        const float g = g0 * std::sqrt (a);
        c.a1 = 1.0f / (1.0f + g * (g + shelfK));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        c.m0 = a * a;
        c.m1 = shelfK * (1.0f - a) * a;
        c.m2 = 1.0f - a * a;
        return c;
    }

    ShelfCoefficients ShelfCoefficients::lerp (const ShelfCoefficients& x, const ShelfCoefficients& y, float t) noexcept
    {
        ShelfCoefficients c;
        c.a1 = x.a1 + t * (y.a1 - x.a1);
        c.a2 = x.a2 + t * (y.a2 - x.a2);
        c.a3 = x.a3 + t * (y.a3 - x.a3);
        c.m0 = x.m0 + t * (y.m0 - x.m0);
        c.m1 = x.m1 + t * (y.m1 - x.m1);
        c.m2 = x.m2 + t * (y.m2 - x.m2);
        return c;
    }

    double ShelfCoefficients::lowShelfMagnitude (double hz, double fc, double gainDb, double sampleRate) noexcept
    {
        // H(s) = 1 + k(A−1)·s·√A/den + (A²−1)/den with s scaled by the shelf's g.
        const double a = std::pow (10.0, gainDb / 40.0);
        const double f = std::clamp (fc, 1.0, 0.45 * sampleRate);
        const double g = std::tan (pi * f / sampleRate) / std::sqrt (a);
        const std::complex<double> z = std::polar (1.0, 2.0 * pi * hz / sampleRate);
        const auto s = (1.0 / g) * (z - 1.0) / (z + 1.0);
        const auto den = s * s + butterworthK * s + 1.0;
        return std::abs (1.0 + butterworthK * (a - 1.0) * s / den + (a * a - 1.0) / den);
    }

    double ShelfCoefficients::highShelfMagnitude (double hz, double fc, double gainDb, double sampleRate) noexcept
    {
        const double a = std::pow (10.0, gainDb / 40.0);
        const double f = std::clamp (fc, 1.0, 0.45 * sampleRate);
        const double g = std::tan (pi * f / sampleRate) * std::sqrt (a);
        const std::complex<double> z = std::polar (1.0, 2.0 * pi * hz / sampleRate);
        const auto s = (1.0 / g) * (z - 1.0) / (z + 1.0);
        const auto den = s * s + butterworthK * s + 1.0;
        return std::abs (a * a + butterworthK * (1.0 - a) * a * s / den + (1.0 - a * a) / den);
    }
}
