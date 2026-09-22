#include "DSP/SidechainFilter.h"

#include <complex>

namespace heat::dsp
{
    void SidechainFilter::prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        logCutoff.prepare (sampleRate, 30.0f);
        logCutoff.reset (std::log (20.0f));
        reset();
        updateCoefficients();
    }

    void SidechainFilter::reset() noexcept
    {
        for (int ch = 0; ch < maxChannels; ++ch)
            ic1eq[ch] = ic2eq[ch] = 0.0f;
        counter = 0;
    }

    void SidechainFilter::setCutoffImmediate (float hz) noexcept
    {
        logCutoff.reset (std::log (std::max (1.0f, hz)));
        updateCoefficients();
    }

    void SidechainFilter::setCutoff (float hz) noexcept
    {
        logCutoff.setTarget (std::log (std::max (1.0f, hz)));
    }

    float SidechainFilter::getCurrentCutoff() const noexcept
    {
        return std::exp (logCutoff.getCurrent());
    }

    void SidechainFilter::updateCoefficients() noexcept
    {
        const double fc = std::min (static_cast<double> (std::exp (logCutoff.getCurrent())), fs * 0.45);
        g = static_cast<float> (std::tan (pi * fc / fs));
        k = 1.41421356f; // 1/Q, Q = 0.7071 (Butterworth)
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void SidechainFilter::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            if (counter == 0)
            {
                if (logCutoff.isSmoothing())
                {
                    logCutoff.skip (updateInterval);
                    updateCoefficients();
                }
                counter = updateInterval;
            }
            --counter;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float v0 = channels[ch][i];
                const float v3 = v0 - ic2eq[ch];
                const float v1 = a1 * ic1eq[ch] + a2 * v3;
                const float v2 = ic2eq[ch] + a2 * ic1eq[ch] + a3 * v3;
                ic1eq[ch] = 2.0f * v1 - ic1eq[ch];
                ic2eq[ch] = 2.0f * v2 - ic2eq[ch];
                channels[ch][i] = v0 - k * v1 - v2; // high-pass output
            }
        }
    }

    double SidechainFilter::magnitudeAt (double hz, double cutoffHz, double sampleRate) noexcept
    {
        // Bilinear-transformed 2nd-order Butterworth HPF (what the TPT SVF implements).
        const double gg = std::tan (pi * cutoffHz / sampleRate);
        const double kk = 1.41421356237;
        const std::complex<double> z = std::polar (1.0, 2.0 * pi * hz / sampleRate);
        const std::complex<double> s = (1.0 / gg) * (z - 1.0) / (z + 1.0);
        const std::complex<double> h = (s * s) / (s * s + kk * s + 1.0);
        return std::abs (h);
    }
}
