#include "TestFramework.h"
#include "EngineHarness.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    int impulseDelay (const EngineParams& p, double fs)
    {
        const int n = 4096;
        StereoBuffer in (n);
        in.l[100] = in.r[100] = 0.5f;
        // Warm-up so the colour path (if any) is running: run a block of
        // silence first by prepending zeros.
        StereoBuffer padded (n + 8192);
        std::copy (in.l.begin(), in.l.end(), padded.l.begin() + 8192);
        std::copy (in.r.begin(), in.r.end(), padded.r.begin() + 8192);
        const auto out = runEngine (p, padded, fs);
        int best = -1;
        float bestAbs = 0.0f;
        for (int i = 8192; i < n + 8192; ++i)
            if (std::abs (out.l[static_cast<size_t> (i)]) > bestAbs)
            {
                bestAbs = std::abs (out.l[static_cast<size_t> (i)]);
                best = i - 8192 - 100;
            }
        return best;
    }
}

HEAT_TEST ("MixAlignment", "neutral processing is a bit-exact delayed copy at any MIX")
{
    const double fs = 48000.0;
    const int n = 48000;
    Noise noise (11);
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
    {
        in.l[static_cast<size_t> (i)] = 0.5f * noise.next();
        in.r[static_cast<size_t> (i)] = 0.5f * noise.next();
    }

    for (float mixValue : { 0.0f, 0.3f, 0.5f, 1.0f })
    {
        auto p = neutralParams();
        p.mix = mixValue;
        HeatEngine e;
        const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
        const int lat = e.getLatencySamples();
        double err = 0.0;
        for (int i = lat; i < n; ++i)
            err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
        note ("MIX %.1f: null residual %.2e (latency %.0f samples)", mixValue, err, lat);
        CHECK (err < 1.0e-6);
    }
}

HEAT_TEST ("MixAlignment", "reported latency equals measured latency for every quality")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        HeatEngine e;
        auto p = neutralParams();
        e.setParams (p);
        e.prepare (fs, 256, 2);
        const int reported = e.getLatencySamples();

        // Neutral (delay path).
        const int measuredNeutral = impulseDelay (p, fs);
        CHECK (measuredNeutral == reported);

        // Oversampled colour path at each quality (tiny TUBE keeps it linear).
        for (int q = 0; q < 3; ++q)
        {
            auto pc = p;
            pc.tube = 0.001f;
            pc.quality = static_cast<Quality> (q);
            const int measured = impulseDelay (pc, fs);
            note ("fs %.0f quality %.0f: reported %.0f, measured %.0f", fs, q, reported, measured);
            CHECK (measured == reported);
        }
    }
}

HEAT_TEST ("MixAlignment", "parallel mix of oversampled wet and dry does not comb-filter")
{
    // Linear-ish colour path (tiny TUBE) blended 50/50 with dry: the combined
    // magnitude response must be flat — any misalignment would notch.
    const double fs = 48000.0;
    auto p = neutralParams();
    p.tube = 0.001f;
    p.mix = 0.5f;
    p.quality = Quality::ultra;
    double worst = 0.0;
    for (double hz : { 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 18000.0 })
    {
        const int n = 32768;
        auto in = stereoSine (fs, hz, 0.25, 0.25, n);
        const auto out = runEngine (p, in, fs);
        const double a = toneAmplitude (out.l.data() + n / 2, n / 2, fs, hz);
        const double db = toDb (a / 0.25);
        worst = std::max (worst, std::abs (db));
        note ("50%% mix @ %.0f Hz: %.3f dB", hz, db);
    }
    CHECK (worst < 0.1);
}
