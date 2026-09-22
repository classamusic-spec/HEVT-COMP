#include "DSP/TruePeakLimiter.h"
#include "DSP/DspMath.h"

namespace heat::dsp
{
    namespace
    {
        // acc[p] = Σj c[j·8 + p] · x[j] for the 8 interleaved phases. Written
        // with vector extensions where available (two 4-lane accumulators
        // stay in registers); plain loop elsewhere.
        inline void accumulatePhases (const float* c, const float* x, float* acc) noexcept
        {
#if defined(__GNUC__) || defined(__clang__)
            typedef float v4 __attribute__ ((vector_size (16)));
            v4 a0 {}, a1 {};
            for (int j = 0; j < TruePeakLimiter::tapsPerPhase; ++j)
            {
                v4 c0, c1;
                __builtin_memcpy (&c0, c + j * 8, sizeof (v4));
                __builtin_memcpy (&c1, c + j * 8 + 4, sizeof (v4));
                const float xv = x[j];
                a0 += c0 * xv;
                a1 += c1 * xv;
            }
            __builtin_memcpy (acc, &a0, sizeof (v4));
            __builtin_memcpy (acc + 4, &a1, sizeof (v4));
#else
            for (int j = 0; j < TruePeakLimiter::tapsPerPhase; ++j)
                for (int p = 0; p < 8; ++p)
                    acc[p] += c[j * 8 + p] * x[j];
#endif
        }

        double besselI0 (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 40; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        }
    }

    void TruePeakLimiter::prepare (double sampleRate)
    {
        fs = sampleRate;
        window = std::max (8, static_cast<int> (std::lround (attackMs * 1.0e-3 * fs)));
        releaseCoeff = -std::expm1 (-1.0 / (releaseMs * 1.0e-3 * fs));

        // Windowed-sinc fractional-delay interpolators for f = 1/8 … 7/8.
        // Taps sit at positions j − (T/2 − 1) − f relative to the fractional point.
        interp.assign (static_cast<size_t> (tapsPerPhase * 8), 0.0f);
        const double beta = 6.0;
        const double half = 0.5 * tapsPerPhase;
        for (int p = 1; p < phases; ++p)
        {
            const double f = static_cast<double> (p) / phases;
            double sum = 0.0;
            std::vector<double> h (static_cast<size_t> (tapsPerPhase));
            for (int j = 0; j < tapsPerPhase; ++j)
            {
                const double d = j - (half - 1.0) - f;
                const double sinc = std::abs (d) < 1.0e-12 ? 1.0 : std::sin (pi * d) / (pi * d);
                const double r = d / half;
                const double w = std::abs (r) < 1.0 ? besselI0 (beta * std::sqrt (1.0 - r * r)) / besselI0 (beta) : 0.0;
                h[static_cast<size_t> (j)] = sinc * w;
                sum += sinc * w;
            }
            for (int j = 0; j < tapsPerPhase; ++j)
                interp[static_cast<size_t> (j * 8 + (p - 1))] = static_cast<float> (h[static_cast<size_t> (j)] / sum);
        }

        for (auto& h : history)
            h.assign (static_cast<size_t> (2 * tapsPerPhase), 0.0f);

        int cap = 1;
        while (cap < window + 2)
            cap <<= 1;
        dqMask = cap - 1;
        dqValue.assign (static_cast<size_t> (cap), 1.0f);
        dqIndex.assign (static_cast<size_t> (cap), 0);
        box.assign (static_cast<size_t> (window), 1.0f);
        reset();
    }

    void TruePeakLimiter::reset() noexcept
    {
        for (auto& h : history)
            std::fill (h.begin(), h.end(), 0.0f);
        histPos = 0;
        prevRegion = 0.0f;
        released = 1.0;
        dqHead = dqTail = 0;
        counter = 0;
        std::fill (box.begin(), box.end(), 1.0f);
        boxPos = 0;
        belowUnity = 0;
        boxSum = static_cast<double> (window);
    }

    void TruePeakLimiter::setCeilingDb (float db) noexcept
    {
        ceiling = dbToGain (std::clamp (db, -24.0f, 0.0f));
    }

    float TruePeakLimiter::process (const float* frame, int numChannels) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);

        // --- Detection ---------------------------------------------------------------
        // After writing x(n) the history holds x(n − T + 1 … n); the interpolated
        // region lies between x(n − T/2) and x(n − T/2 + 1).
        float region = 0.0f, sampleAtM = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& h = history[ch];
            h[static_cast<size_t> (histPos)] = frame[ch];
            h[static_cast<size_t> (histPos + tapsPerPhase)] = frame[ch];
            const float* x = h.data() + histPos + 1; // oldest … newest

            float acc[8] {};
            accumulatePhases (interp.data(), x, acc);
            for (int p = 0; p < phases - 1; ++p)
                region = std::max (region, std::abs (acc[p]));

            // |x(m)| with m = n − T/2 (left sample of the new region).
            sampleAtM = std::max (sampleAtM, std::abs (x[tapsPerPhase / 2 - 1]));
        }
        if (++histPos == tapsPerPhase)
            histPos = 0;

        // Peak around m: the sample and the regions on both of its sides.
        const float peak = std::max ({ sampleAtM, prevRegion, region });
        prevRegion = region;

        const double required = peak > ceiling ? static_cast<double> (ceiling) / peak : 1.0;

        // --- Release (lower envelope) -------------------------------------------------
        if (required <= released)
            released = required;
        else
        {
            released += releaseCoeff * (required - released);
            if (required >= 1.0 && released > 1.0 - 1.0e-6)
                released = 1.0;
        }
        const float releasedF = released >= 1.0 ? 1.0f : std::min (static_cast<float> (released), static_cast<float> (required));

        // --- Sliding minimum over W + 1 samples ---------------------------------------
        while (dqTail > dqHead && dqValue[static_cast<size_t> ((dqTail - 1) & dqMask)] >= releasedF)
            --dqTail;
        dqValue[static_cast<size_t> (dqTail & dqMask)] = releasedF;
        dqIndex[static_cast<size_t> (dqTail & dqMask)] = counter;
        ++dqTail;
        while (dqIndex[static_cast<size_t> (dqHead & dqMask)] <= counter - (window + 1))
            ++dqHead;
        ++counter;
        const float held = dqValue[static_cast<size_t> (dqHead & dqMask)];

        // --- Box average over W samples -----------------------------------------------
        const float old = box[static_cast<size_t> (boxPos)];
        box[static_cast<size_t> (boxPos)] = held;
        if (++boxPos == window)
            boxPos = 0;
        belowUnity += (held < 1.0f ? 1 : 0) - (old < 1.0f ? 1 : 0);

        if (belowUnity == 0)
        {
            boxSum = static_cast<double> (window);
            return 1.0f;
        }
        boxSum += static_cast<double> (held) - static_cast<double> (old);
        return static_cast<float> (boxSum / window);
    }
}
