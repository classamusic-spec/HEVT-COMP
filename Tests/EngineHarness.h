#pragma once

#include "DSP/HeatEngine.h"
#include "TestSignals.h"

#include <functional>
#include <vector>

namespace heat::test
{
    struct StereoBuffer
    {
        std::vector<float> l, r;

        explicit StereoBuffer (int n = 0) : l (static_cast<size_t> (n), 0.0f), r (static_cast<size_t> (n), 0.0f) {}
        int size() const { return static_cast<int> (l.size()); }
    };

    // Processes a stereo buffer through a freshly prepared HeatEngine.
    // `perBlock` (optional) may change parameters before each block.
    inline StereoBuffer runEngine (dsp::EngineParams params, StereoBuffer input, double fs = 48000.0,
                                   int blockSize = 256, const StereoBuffer* sidechain = nullptr,
                                   const std::function<void (int blockStart, dsp::EngineParams&)>& perBlock = {},
                                   dsp::HeatEngine* engineOut = nullptr)
    {
        dsp::HeatEngine localEngine;
        dsp::HeatEngine& engine = engineOut != nullptr ? *engineOut : localEngine;
        engine.setParams (params);
        engine.prepare (fs, blockSize, 2);

        const int n = input.size();
        for (int start = 0; start < n; start += blockSize)
        {
            const int len = std::min (blockSize, n - start);
            if (perBlock)
            {
                perBlock (start, params);
                engine.setParams (params);
            }
            float* io[2] = { input.l.data() + start, input.r.data() + start };
            const float* sc[2] = { nullptr, nullptr };
            if (sidechain != nullptr)
            {
                sc[0] = sidechain->l.data() + start;
                sc[1] = sidechain->r.data() + start;
            }
            engine.process (io, 2, sidechain != nullptr ? sc : nullptr, sidechain != nullptr ? 2 : 0, len);
        }
        return input;
    }

    inline StereoBuffer stereoSine (double fs, double hz, double ampL, double ampR, int n)
    {
        StereoBuffer b (n);
        b.l = sine (fs, hz, ampL, n);
        b.r = sine (fs, hz, ampR, n);
        return b;
    }

    // Neutral parameter set: nothing but a latency-aligned pass-through.
    inline dsp::EngineParams neutralParams()
    {
        dsp::EngineParams p;
        p.mode = dsp::Mode::clean;
        p.compress = 0.0f;
        p.tube = 0.0f;
        p.iron = 0.0f;
        p.mix = 1.0f;
        p.inputDb = 0.0f;
        p.outputDb = 0.0f;
        p.autoMakeup = true;
        return p;
    }

    // Largest absolute sample-to-sample step (first difference) in a range.
    inline double maxStep (const std::vector<float>& x, int start, int end)
    {
        double m = 0.0;
        for (int i = std::max (1, start); i < end; ++i)
            m = std::max (m, (double) std::abs (x[static_cast<size_t> (i)] - x[static_cast<size_t> (i - 1)]));
        return m;
    }

    // Largest absolute second difference — spikes here are clicks.
    inline double maxSecondDifference (const std::vector<float>& x, int start, int end)
    {
        double m = 0.0;
        end = std::min (end, static_cast<int> (x.size()));
        for (int i = std::max (2, start); i < end; ++i)
        {
            const auto k = static_cast<size_t> (i);
            m = std::max (m, (double) std::abs (x[k] - 2.0f * x[k - 1] + x[k - 2]));
        }
        return m;
    }

    // Click measure around a parameter change at sample `at`: the largest
    // second difference during the transition (`window` samples) relative to
    // the steady state on both sides of it. A pure level or timbre change
    // stays ≤ ~1 (it moves between the two); a discontinuity spikes above.
    inline double clickRatio (const std::vector<float>& x, int at, int window)
    {
        const double before = maxSecondDifference (x, at - 6000, at - 500);
        const double after = maxSecondDifference (x, at + window + 500, at + window + 6000);
        const double during = maxSecondDifference (x, at - 256, at + window);
        return during / std::max (1.0e-12, std::max (before, after));
    }
}
