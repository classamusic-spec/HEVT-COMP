#include "TestFramework.h"
#include "CompressorHarness.h"
#include "EngineHarness.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    CompressorRun runLinked (StereoLink link)
    {
        const double fs = 48000.0;
        const int n = static_cast<int> (fs);
        const auto left = sine (fs, 440.0, 0.7, n);   // hot
        const auto right = sine (fs, 440.0, 0.02, n); // quiet
        CompressorEngine::Controls c;
        c.mode = Mode::clean;
        c.compress = 0.7f;
        c.link = link;
        return runCompressor (c, left, &right, fs);
    }
}

HEAT_TEST ("Stereo", "LINKED applies identical gain to both channels")
{
    const auto run = runLinked (StereoLink::linked);
    double maxDiff = 0.0;
    for (size_t i = 0; i < run.grL.size(); ++i)
        maxDiff = std::max (maxDiff, (double) std::abs (run.grL[i] - run.grR[i]));
    note ("linked: max L/R GR difference %.6f dB, end GR %.2f dB", maxDiff, run.grL.back());
    CHECK (maxDiff < 1.0e-5);
    CHECK (run.grL.back() < -3.0f);
}

HEAT_TEST ("Stereo", "DUAL MONO compresses channels independently")
{
    const auto run = runLinked (StereoLink::dualMono);
    note ("dual mono: L GR %.2f dB, R GR %.2f dB", run.grL.back(), run.grR.back());
    CHECK (run.grL.back() < -3.0f);
    CHECK (run.grR.back() > -0.05f);
}

HEAT_TEST ("Stereo", "PARTIAL link sits between linked and dual mono")
{
    const auto linked = runLinked (StereoLink::linked);
    const auto partial = runLinked (StereoLink::partial);
    const auto dual = runLinked (StereoLink::dualMono);
    note ("quiet channel GR: linked %.2f, partial %.2f, dual %.2f dB", linked.grR.back(), partial.grR.back(), dual.grR.back());
    CHECK (partial.grR.back() > linked.grR.back() + 0.5f);
    CHECK (partial.grR.back() < dual.grR.back() - 0.5f);
}

HEAT_TEST ("Stereo", "linked image does not wander (L/R ratio preserved)")
{
    // Program with changing left/right balance through the full engine: the
    // output L/R level ratio must equal the input ratio at every point.
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 2);
    StereoBuffer in (n);
    Noise noise (5);
    for (int i = 0; i < n; ++i)
    {
        const float s = noise.next() * static_cast<float> (0.3 + 0.3 * std::sin (2 * pi * 1.5 * i / fs));
        const float pan = static_cast<float> (0.5 + 0.45 * std::sin (2 * pi * 0.7 * i / fs));
        in.l[static_cast<size_t> (i)] = s * pan;
        in.r[static_cast<size_t> (i)] = s * (1.0f - pan);
    }
    auto p = neutralParams();
    p.compress = 0.8f;
    p.attackMs = 1.0f;
    p.releaseMs = 60.0f;
    const auto out = runEngine (p, in, fs);
    {
        HeatEngine e;
        e.setParams (p);
        e.prepare (fs, 256, 2);
        const int lat = e.getLatencySamples();
        double worst = 0.0;
        for (int i = lat + 1000; i < n; ++i)
        {
            const float inL = in.l[static_cast<size_t> (i - lat)], inR = in.r[static_cast<size_t> (i - lat)];
            if (std::abs (inL) < 1.0e-3f || std::abs (inR) < 1.0e-3f)
                continue;
            const double gL = out.l[static_cast<size_t> (i)] / inL;
            const double gR = out.r[static_cast<size_t> (i)] / inR;
            worst = std::max (worst, std::abs (toDb (std::abs (gL)) - toDb (std::abs (gR))));
        }
        note ("worst per-sample L/R gain mismatch: %.6f dB", worst);
        CHECK (worst < 0.01);
    }
}

HEAT_TEST ("Stereo", "mono input processes correctly")
{
    const double fs = 48000.0;
    const int n = 48000;
    auto x = sine (fs, 1000.0, 0.5, n);
    HeatEngine e;
    auto p = neutralParams();
    p.compress = 0.6f;
    e.setParams (p);
    e.prepare (fs, 512, 1);
    for (int s = 0; s < n; s += 512)
    {
        float* io[1] = { x.data() + s };
        e.process (io, 1, nullptr, 0, std::min (512, n - s));
    }
    note ("mono: GR %.2f dB, output peak %.3f", e.getTelemetry().grDb[0], peakOf (x.data() + n - 4800, 4800));
    CHECK (e.getTelemetry().grDb[0] < -2.0f);
    CHECK (std::isfinite (x.back()));
}
