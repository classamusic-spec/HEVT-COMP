#include "DSP/SidechainFilter.h"

#include <complex>

namespace heat::dsp
{
    namespace
    {
        constexpr float smoothingMs = 30.0f;
        constexpr double butterworthK = 1.41421356237; // 1/Q, Q = 0.7071

        std::complex<double> sOf (double hz, double cutoffHz, double sampleRate)
        {
            // Bilinear transform with the cutoff pre-warped (what the TPT SVF implements).
            const double gg = std::tan (pi * cutoffHz / sampleRate);
            const std::complex<double> z = std::polar (1.0, 2.0 * pi * hz / sampleRate);
            return (1.0 / gg) * (z - 1.0) / (z + 1.0);
        }

        double clampFc (double fc, double sampleRate) { return std::clamp (fc, 1.0, 0.45 * sampleRate); }
    }

    void SidechainFilter::Svf::design (double fc, double sampleRate, double kk) noexcept
    {
        g = static_cast<float> (std::tan (pi * clampFc (fc, sampleRate) / sampleRate));
        k = static_cast<float> (kk);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void SidechainFilter::Svf::reset() noexcept
    {
        for (int ch = 0; ch < maxChannels; ++ch)
            ic1eq[ch] = ic2eq[ch] = 0.0f;
    }

    float SidechainFilter::lowpassAmountFor (float hz) noexcept
    {
        if (hz >= lpfOffHz)
            return 0.0f;
        if (hz <= lpfFadeStartHz)
            return 1.0f;
        return std::log (lpfOffHz / hz) / std::log (lpfOffHz / lpfFadeStartHz);
    }

    void SidechainFilter::prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto* s : { &logCutoff, &logLowpass, &logBellHz, &bellDb, &logBellQ })
            s->prepare (sampleRate, smoothingMs);
        logCutoff.reset (std::log (20.0f));
        logLowpass.reset (std::log (lpfOffHz));
        logBellHz.reset (std::log (3000.0f));
        bellDb.reset (0.0f);
        logBellQ.reset (0.0f);
        reset();
        updateCoefficients();
    }

    void SidechainFilter::reset() noexcept
    {
        hpf.reset();
        lpf.reset();
        bell.reset();
        counter = 0;
    }

    void SidechainFilter::setCutoffImmediate (float hz) noexcept
    {
        logCutoff.reset (std::log (std::max (1.0f, hz)));
        updateCoefficients();
    }

    void SidechainFilter::setLowpassImmediate (float hz) noexcept
    {
        logLowpass.reset (std::log (std::max (1.0f, hz)));
        updateCoefficients();
    }

    void SidechainFilter::setBellImmediate (float hz, float gainDb, float q) noexcept
    {
        logBellHz.reset (std::log (std::max (1.0f, hz)));
        bellDb.reset (gainDb);
        logBellQ.reset (std::log (std::max (0.05f, q)));
        updateCoefficients();
    }

    void SidechainFilter::setCutoff (float hz) noexcept   { logCutoff.setTarget (std::log (std::max (1.0f, hz))); }
    void SidechainFilter::setLowpass (float hz) noexcept  { logLowpass.setTarget (std::log (std::max (1.0f, hz))); }

    void SidechainFilter::setBell (float hz, float gainDb, float q) noexcept
    {
        logBellHz.setTarget (std::log (std::max (1.0f, hz)));
        bellDb.setTarget (gainDb);
        logBellQ.setTarget (std::log (std::max (0.05f, q)));
    }

    float SidechainFilter::getCurrentCutoff() const noexcept
    {
        return std::exp (logCutoff.getCurrent());
    }

    bool SidechainFilter::isSmoothing() const noexcept
    {
        return logCutoff.isSmoothing() || logLowpass.isSmoothing() || logBellHz.isSmoothing()
               || bellDb.isSmoothing() || logBellQ.isSmoothing();
    }

    void SidechainFilter::updateCoefficients() noexcept
    {
        hpf.design (std::exp (logCutoff.getCurrent()), fs, butterworthK);

        const float lpHz = std::exp (logLowpass.getCurrent());
        lpfAmount = lowpassAmountFor (lpHz);
        lpf.design (lpHz, fs, butterworthK);

        // Peaking bell (Simper SVF): A = 10^(dB/40), k = 1/(Q·A), out = x + k(A²−1)·bp.
        const double a = std::pow (10.0, bellDb.getCurrent() / 40.0);
        const double q = std::exp (logBellQ.getCurrent());
        bell.design (std::exp (logBellHz.getCurrent()), fs, 1.0 / (q * a));
        bellM1 = static_cast<float> ((1.0 / (q * a)) * (a * a - 1.0));
    }

    void SidechainFilter::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            if (counter == 0)
            {
                if (isSmoothing())
                {
                    for (auto* s : { &logCutoff, &logLowpass, &logBellHz, &bellDb, &logBellQ })
                        s->skip (updateInterval);
                    updateCoefficients();
                }
                counter = updateInterval;
            }
            --counter;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                // Every section keeps running while bypassed so it can fade
                // in from a live state.
                float v1, v2;
                const float x = channels[ch][i];
                hpf.tick (ch, x, v1, v2);
                float y = x - hpf.k * v1 - v2; // high-pass output

                lpf.tick (ch, y, v1, v2);
                y += lpfAmount * (v2 - y);

                bell.tick (ch, y, v1, v2);
                y += bellM1 * v1;
                channels[ch][i] = y;
            }
        }
    }

    double SidechainFilter::magnitudeAt (double hz, double cutoffHz, double sampleRate) noexcept
    {
        const auto s = sOf (hz, clampFc (cutoffHz, sampleRate), sampleRate);
        return std::abs ((s * s) / (s * s + butterworthK * s + 1.0));
    }

    double SidechainFilter::lowpassMagnitudeAt (double hz, double cutoffHz, double sampleRate) noexcept
    {
        const auto s = sOf (hz, clampFc (cutoffHz, sampleRate), sampleRate);
        return std::abs (1.0 / (s * s + butterworthK * s + 1.0));
    }

    double SidechainFilter::bellMagnitudeAt (double hz, double centreHz, double gainDb, double q, double sampleRate) noexcept
    {
        const double a = std::pow (10.0, gainDb / 40.0);
        const double kk = 1.0 / (q * a);
        const auto s = sOf (hz, clampFc (centreHz, sampleRate), sampleRate);
        const auto den = s * s + kk * s + 1.0;
        return std::abs (1.0 + kk * (a * a - 1.0) * s / den);
    }

    double SidechainFilter::chainMagnitudeAt (double hz, double hpfHz, double lpfHz, double bellHz, double bellDb,
                                              double bellQ, double sampleRate) noexcept
    {
        const auto sh = sOf (hz, clampFc (hpfHz, sampleRate), sampleRate);
        std::complex<double> h = (sh * sh) / (sh * sh + butterworthK * sh + 1.0);

        const double wet = lowpassAmountFor (static_cast<float> (lpfHz));
        if (wet > 0.0)
        {
            const auto sl = sOf (hz, clampFc (lpfHz, sampleRate), sampleRate);
            h *= 1.0 + wet * (1.0 / (sl * sl + butterworthK * sl + 1.0) - 1.0);
        }

        if (std::abs (bellDb) > 0.0)
        {
            const double a = std::pow (10.0, bellDb / 40.0);
            const double kk = 1.0 / (bellQ * a);
            const auto sb = sOf (hz, clampFc (bellHz, sampleRate), sampleRate);
            h *= 1.0 + kk * (a * a - 1.0) * sb / (sb * sb + kk * sb + 1.0);
        }
        return std::abs (h);
    }
}
