#include "TestFramework.h"
#include "EngineHarness.h"

#include <random>

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    EngineParams everythingOn()
    {
        auto p = neutralParams();
        p.compress = 0.65f;
        p.mode = Mode::drive;
        p.detector = Detector::rms;
        p.tube = 0.4f;
        p.iron = 0.6f;
        p.mix = 0.8f;
        p.hpfHz = 80.0f;
        p.stereoMode = StereoMode::midSide;
        p.link = StereoLink::partial;
        p.lookaheadMs = 2.0f;
        p.scLpfHz = 9000.0f;
        p.scEqHz = 5000.0f;
        p.scEqDb = 6.0f;
        p.scEqQ = 2.0f;
        p.limiter = true;
        p.ceilingDb = -0.5f;
        p.ironModel = IronModel::hysteresis;
        p.multiband = MultibandMode::threeBand;
        p.bandAmount[0] = 1.3f;
        p.bandAmount[2] = 0.7f;
        return p;
    }

    StereoBuffer programme (int n, uint32_t seed)
    {
        StereoBuffer b (n);
        Noise noise (seed);
        float lpL = 0.0f, lpR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            lpL += 0.02f * (noise.next() - lpL);
            lpR += 0.02f * (noise.next() - lpR);
            const float hit = (i % 6000) < 300 ? 0.7f * noise.next() : 0.0f;
            b.l[static_cast<size_t> (i)] = 2.0f * lpL + 0.1f * noise.next() + hit;
            b.r[static_cast<size_t> (i)] = 2.0f * lpR + 0.1f * noise.next() + hit;
        }
        return b;
    }
}

HEAT_TEST ("FeatureMatrix", "every 2.1 feature at once: output independent of host block size")
{
    const double fs = 48000.0;
    const int n = 48000;
    const auto in = programme (n, 31);
    const auto p = everythingOn();
    const auto reference = runEngine (p, in, fs, 256);
    double worst = 0.0;
    for (int bs : { 1, 17, 64, 1000, 4096 })
    {
        const auto out = runEngine (p, in, fs, bs);
        for (int i = 0; i < n; ++i)
            worst = std::max ({ worst, (double) std::abs (out.l[static_cast<size_t> (i)] - reference.l[static_cast<size_t> (i)]),
                                (double) std::abs (out.r[static_cast<size_t> (i)] - reference.r[static_cast<size_t> (i)]) });
    }
    note ("max deviation across host block sizes 1…4096: %.2e", worst);
    CHECK (worst < 1.0e-6);
}

HEAT_TEST ("FeatureMatrix", "fuzz: random 2.1 settings every block stay finite and bounded")
{
    const double fs = 48000.0;
    const int n = 240000;
    const auto in = programme (n, 77);
    std::mt19937 rng (123);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    std::vector<EngineParams> settings;
    for (int i = 0; i < 400; ++i)
    {
        auto p = everythingOn();
        p.compress = u (rng);
        p.mode = static_cast<Mode> (static_cast<int> (u (rng) * 2.99f));
        p.detector = static_cast<Detector> (static_cast<int> (u (rng) * 2.99f));
        p.tube = u (rng) < 0.5f ? 0.0f : u (rng);
        p.iron = u (rng) < 0.5f ? 0.0f : u (rng);
        p.mix = u (rng);
        p.inputDb = -24.0f + 48.0f * u (rng);
        p.outputDb = -24.0f + 48.0f * u (rng);
        p.stereoMode = static_cast<StereoMode> (static_cast<int> (u (rng) * 3.99f));
        p.link = static_cast<StereoLink> (static_cast<int> (u (rng) * 2.99f));
        const float lookaheads[6] { 0.0f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
        p.lookaheadMs = lookaheads[static_cast<int> (u (rng) * 5.99f)];
        p.scLpfHz = 1000.0f * std::pow (20.0f, u (rng));
        p.scEqHz = 80.0f * std::pow (150.0f, u (rng));
        p.scEqDb = -18.0f + 36.0f * u (rng);
        p.scEqQ = 0.3f * std::pow (26.0f, u (rng));
        p.limiter = u (rng) < 0.5f;
        p.ceilingDb = -12.0f * u (rng);
        p.ironModel = u (rng) < 0.5f ? IronModel::classic : IronModel::hysteresis;
        p.multiband = static_cast<MultibandMode> (static_cast<int> (u (rng) * 2.99f));
        p.xoverLowHz = 40.0f * std::pow (25.0f, u (rng));
        p.xoverHighHz = 1000.0f * std::pow (12.0f, u (rng));
        for (auto& a : p.bandAmount)
            a = 2.0f * u (rng);
        p.quality = static_cast<Quality> (static_cast<int> (u (rng) * 2.99f));
        p.bypass = u (rng) < 0.05f;
        settings.push_back (p);
    }
    std::uniform_int_distribution<int> pick (0, static_cast<int> (settings.size()) - 1);
    const auto out = runEngine (everythingOn(), in, fs, 600, nullptr, [&] (int, EngineParams& q) { q = settings[static_cast<size_t> (pick (rng))]; });
    bool finite = true;
    double peak = 0.0;
    for (int i = 0; i < n; ++i)
    {
        finite = finite && std::isfinite (out.l[static_cast<size_t> (i)]) && std::isfinite (out.r[static_cast<size_t> (i)]);
        peak = std::max ({ peak, (double) std::abs (out.l[static_cast<size_t> (i)]), (double) std::abs (out.r[static_cast<size_t> (i)]) });
    }
    note ("400 random settings, changed every 600 samples for 5 s: output peak %.2f (%.1f dBFS)", peak, toDb (peak));
    CHECK (finite);
    CHECK (peak <= 31.6);
}

HEAT_TEST ("FeatureMatrix", "reported latency follows LOOKAHEAD / LIMITER changes at any time")
{
    HeatEngine e;
    auto p = neutralParams();
    e.setParams (p);
    e.prepare (48000.0, 512, 2);
    const int core = e.getCoreLatencySamples();
    CHECK (e.getLatencySamples() == core);
    p.lookaheadMs = 5.0f;
    e.setParams (p);
    CHECK (e.getLatencySamples() == core + 240);
    p.limiter = true;
    e.setParams (p);
    CHECK (e.getLatencySamples() == core + 240 + e.getLimiterLatencySamples());
    CHECK (e.latencyFor (0.0f, false) == core);
    note ("48 kHz: core %.0f, limiter %.0f samples (%.2f ms)", core, e.getLimiterLatencySamples(),
          e.getLimiterLatencySamples() / 48.0);
}
