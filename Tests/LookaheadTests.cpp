#include "TestFramework.h"
#include "EngineHarness.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    // Position of the output impulse relative to the input impulse.
    int measuredLatency (const EngineParams& p, double fs)
    {
        const int pad = 8192, n = 8192;
        StereoBuffer in (pad + n);
        in.l[static_cast<size_t> (pad + 100)] = in.r[static_cast<size_t> (pad + 100)] = 0.25f;
        const auto out = runEngine (p, in, fs);
        int best = -1;
        float bestAbs = 0.0f;
        for (int i = pad; i < pad + n; ++i)
            if (std::abs (out.l[static_cast<size_t> (i)]) > bestAbs)
            {
                bestAbs = std::abs (out.l[static_cast<size_t> (i)]);
                best = i - pad - 100;
            }
        return best;
    }
}

HEAT_TEST ("Lookahead", "reported latency equals measured latency with LOOKAHEAD and LIMITER")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (float la : { 0.0f, 0.5f, 2.0f, 10.0f })
            for (bool lim : { false, true })
            {
                auto p = neutralParams();
                p.lookaheadMs = la;
                p.limiter = lim;
                HeatEngine e;
                e.setParams (p);
                e.prepare (fs, 256, 2);
                const int reported = e.getLatencySamples();
                const int expected = e.getCoreLatencySamples() + static_cast<int> (std::lround (la * 1.0e-3 * fs))
                                     + (lim ? e.getLimiterLatencySamples() : 0);
                CHECK (reported == expected);

                const int measured = measuredLatency (p, fs);
                auto pc = p;
                pc.tube = 0.001f; // oversampled colour path
                const int measuredColour = measuredLatency (pc, fs);
                if (measured != reported || measuredColour != reported)
                    note ("fs %.0f lookahead %.1f limiter %.0f: reported %.0f", fs, la, lim ? 1 : 0, reported);
                CHECK (measured == reported);
                CHECK (measuredColour == reported);
            }
    HeatEngine e;
    auto p = neutralParams();
    p.lookaheadMs = 10.0f;
    p.limiter = true;
    e.setParams (p);
    e.prepare (48000.0, 256, 2);
    note ("48 kHz latency: core %.0f, +10 ms lookahead %.0f, +limiter %.0f samples",
          e.getCoreLatencySamples(), 480, e.getLimiterLatencySamples());
}

HEAT_TEST ("Lookahead", "gain reduction starts before the transient")
{
    // A quiet → loud step. Without look-ahead the first milliseconds of the
    // step pass at full level (attack time); with look-ahead the gain is
    // already down when the step arrives at the output.
    const double fs = 48000.0;
    const int n = 48000, step = 24000;
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
    {
        const float a = i < step ? 0.01f : 0.9f;
        in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] = a * static_cast<float> (std::sin (2.0 * pi * 1000.0 * i / fs));
    }

    auto overshootFor = [&] (float lookaheadMs)
    {
        auto p = neutralParams();
        p.compress = 0.8f;
        p.attackMs = 2.0f;
        p.releaseMs = 200.0f;
        p.autoMakeup = false;
        p.lookaheadMs = lookaheadMs;
        HeatEngine e;
        const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
        const int lat = e.getLatencySamples();
        // Peak of the first 2 ms of the step vs the settled level 100 ms later.
        const double onset = peakOf (out.l.data() + step + lat, static_cast<int> (0.002 * fs));
        const double settled = peakOf (out.l.data() + step + lat + 4800, 480);
        return toDb (onset / settled);
    };

    const double without = overshootFor (0.0f);
    const double with2 = overshootFor (2.0f);
    const double with5 = overshootFor (5.0f);
    // The attack is a one-pole in the dB domain, so whatever overshoot is left
    // after a look-ahead of D decays as e^(−D/τ) (τ = 2 ms here).
    note ("onset overshoot above settled level: no look-ahead %+.2f dB, 2 ms %+.2f dB, 5 ms %+.2f dB", without, with2, with5);
    note ("predicted from the attack constant: 2 ms %+.2f dB, 5 ms %+.2f dB", without * std::exp (-1.0), without * std::exp (-2.5));
    CHECK (without > 10.0);
    CHECK_NEAR (with2, without * std::exp (-1.0), 1.0);
    CHECK_NEAR (with5, without * std::exp (-2.5), 0.7);
}

HEAT_TEST ("Lookahead", "changing LOOKAHEAD and LIMITER mid-stream is click-free")
{
    const double fs = 48000.0;
    const int n = 96000;
    auto in = stereoSine (fs, 220.0, 0.3, 0.3, n);
    auto p = neutralParams();
    p.compress = 0.4f;
    p.mode = Mode::warm;
    const auto out = runEngine (p, in, fs, 256, nullptr, [] (int start, EngineParams& q)
    {
        q.lookaheadMs = start >= 24000 && start < 48000 ? 5.0f : start >= 48000 && start < 72000 ? 1.0f : 0.0f;
        q.limiter = start >= 36000 && start < 84000;
    });
    double worst = 0.0;
    for (int at : { 24064, 36096, 48128, 72192, 84224 })
        worst = std::max (worst, clickRatio (out.l, at, 2400));
    bool finite = true;
    for (float v : out.l)
        finite = finite && std::isfinite (v);
    note ("worst click ratio around changes %.2f (1 = steady state)", worst);
    CHECK (finite);
    CHECK (worst < 1.3);
}
