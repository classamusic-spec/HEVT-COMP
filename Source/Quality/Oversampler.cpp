#include "Quality/Oversampler.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace heat::dsp
{
    namespace
    {
        double besselI0 (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 64; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
                if (term < sum * 1.0e-16)
                    break;
            }
            return sum;
        }

        double kaiserBeta (double attenuationDb)
        {
            if (attenuationDb > 50.0)
                return 0.1102 * (attenuationDb - 8.7);
            if (attenuationDb >= 21.0)
                return 0.5842 * std::pow (attenuationDb - 21.0, 0.4) + 0.07886 * (attenuationDb - 21.0);
            return 0.0;
        }
    }

    // ------------------------------------------------------------------------
    void HalfbandStage::design (int taps, double stopbandDb, int maxInputSamples)
    {
        assert ((taps - 3) % 4 == 0);
        numTaps = taps;
        K = (taps - 3) / 4;
        L = 2 * K + 2;
        const int M = (taps - 1) / 2;
        const double beta = kaiserBeta (stopbandDb);
        const double i0Beta = besselI0 (beta);

        g.assign (static_cast<size_t> (L), 0.0f);
        std::vector<double> gd (static_cast<size_t> (L));
        double sum = 0.0;
        for (int j = 0; j < L; ++j)
        {
            const int n = 2 * j;
            const double x = 0.5 * (n - M);                     // half-band: cutoff at fs/4
            const double sinc = std::sin (3.14159265358979323846 * x) / (3.14159265358979323846 * x);
            const double r = 2.0 * n / (taps - 1) - 1.0;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0Beta;
            gd[static_cast<size_t> (j)] = 0.5 * sinc * w;
            sum += gd[static_cast<size_t> (j)];
        }
        // Exact unity DC gain: even taps sum to 0.5, centre tap is 0.5.
        for (int j = 0; j < L; ++j)
            g[static_cast<size_t> (j)] = static_cast<float> (gd[static_cast<size_t> (j)] * 0.5 / sum);

        (void) maxInputSamples;
        upHist.assign (static_cast<size_t> (2 * L), 0.0f);
        downHist.assign (static_cast<size_t> (2 * L), 0.0f);
        oddHist.assign (static_cast<size_t> (K + 2), 0.0f);
        reset();
    }

    void HalfbandStage::reset() noexcept
    {
        std::fill (upHist.begin(), upHist.end(), 0.0f);
        std::fill (downHist.begin(), downHist.end(), 0.0f);
        std::fill (oddHist.begin(), oddHist.end(), 0.0f);
        upPos = downPos = oddPos = 0;
    }

    void HalfbandStage::upsample (const float* in, float* out, int n) noexcept
    {
        const float* coeffs = g.data();
        float* hist = upHist.data();

        for (int m = 0; m < n; ++m)
        {
            upPos = (upPos == 0 ? L : upPos) - 1;
            hist[upPos] = hist[upPos + L] = in[m];

            const float* h = hist + upPos; // newest first
            float acc = 0.0f;
            for (int j = 0; j < L; ++j)
                acc += coeffs[j] * h[j];

            out[2 * m] = 2.0f * acc;
            out[2 * m + 1] = h[K];
        }
    }

    void HalfbandStage::downsample (const float* in, float* out, int n) noexcept
    {
        const float* coeffs = g.data();
        float* hist = downHist.data();
        const int oddSize = K + 2;

        for (int m = 0; m < n; ++m)
        {
            downPos = (downPos == 0 ? L : downPos) - 1;
            hist[downPos] = hist[downPos + L] = in[2 * m];

            const float* h = hist + downPos;
            float acc = 0.0f;
            for (int j = 0; j < L; ++j)
                acc += coeffs[j] * h[j];

            // vo[m - K - 1]: the odd sample written K + 1 steps ago.
            int readPos = oddPos - (K + 1);
            if (readPos < 0)
                readPos += oddSize;
            const float delayedOdd = oddHist[static_cast<size_t> (readPos)];
            oddHist[static_cast<size_t> (oddPos)] = in[2 * m + 1];
            if (++oddPos == oddSize)
                oddPos = 0;

            out[m] = acc + 0.5f * delayedOdd;
        }
    }

    // ------------------------------------------------------------------------
    void Oversampler::prepare (int maxBlockSize)
    {
        maxBlock = std::max (1, maxBlockSize);
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            for (int s = 0; s < maxStages; ++s)
                stages[ch][s].design (stageTaps[s], stageStopbandDb[s], maxBlock << s);

            for (int s = 0; s <= maxStages; ++s)
                work[ch][s].assign (static_cast<size_t> (maxBlock << s), 0.0f);
        }
    }

    void Oversampler::reset() noexcept
    {
        for (auto& channelStages : stages)
            for (auto& s : channelStages)
                s.reset();
    }

    void Oversampler::setNumStages (int s) noexcept
    {
        numStages = std::clamp (s, 1, maxStages);
    }

    float* Oversampler::upsample (int channel, const float* in, int n) noexcept
    {
        assert (n <= maxBlock);
        const float* src = in;
        for (int s = 0; s < numStages; ++s)
        {
            float* dst = work[channel][s + 1].data();
            stages[channel][s].upsample (src, dst, n << s);
            src = dst;
        }
        return work[channel][numStages].data();
    }

    void Oversampler::downsample (int channel, float* out, int n) noexcept
    {
        assert (n <= maxBlock);
        for (int s = numStages - 1; s >= 0; --s)
        {
            const float* src = work[channel][s + 1].data();
            float* dst = s == 0 ? out : work[channel][s].data();
            stages[channel][s].downsample (src, dst, n << s);
        }
    }

    int Oversampler::getUpLatencyHighRate() const noexcept
    {
        const int factor = getFactor();
        int latency = 0;
        for (int s = 0; s < numStages; ++s)
            latency += stages[0][s].getFilterLatency() * (factor >> (s + 1));
        return latency;
    }

    double Oversampler::getRoundTripLatencyBaseRate (int s) const noexcept
    {
        double latency = 0.0;
        for (int i = 0; i < std::clamp (s, 0, maxStages); ++i)
            latency += (stageTaps[i] - 1) / static_cast<double> (2 << i);
        return latency;
    }
}
