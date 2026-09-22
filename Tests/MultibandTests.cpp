#include "TestFramework.h"
#include "EngineHarness.h"
#include "DSP/Multiband.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    double shelfResponseDb (bool low, double hz, double fc, float gainDb, double fs)
    {
        ShelfState state;
        const float g0 = static_cast<float> (std::tan (pi * fc / fs));
        const auto c = low ? ShelfCoefficients::lowShelf (g0, gainDb) : ShelfCoefficients::highShelf (g0, gainDb);
        const int n = static_cast<int> (fs);
        auto x = sine (fs, hz, 0.25, n);
        for (auto& v : x)
            v = state.process (v, c);
        return toDb (toneAmplitude (x.data() + n / 2, n / 2, fs, hz) / 0.25);
    }

    // Bass-heavy programme: loud 60 Hz with a quiet 3 kHz tone on top.
    StereoBuffer bassAndPresence (double fs, int n)
    {
        StereoBuffer b (n);
        for (int i = 0; i < n; ++i)
        {
            // Bass pulses (every 0.5 s) so gain reduction moves.
            const double t = static_cast<double> (i) / fs;
            const double env = 0.5 + 0.5 * std::sin (2.0 * pi * 2.0 * t);
            const double v = 0.8 * env * std::sin (2.0 * pi * 60.0 * t) + 0.05 * std::sin (2.0 * pi * 3000.0 * t);
            b.l[static_cast<size_t> (i)] = b.r[static_cast<size_t> (i)] = static_cast<float> (v);
        }
        return b;
    }
}

HEAT_TEST ("Multiband", "detector bands are 4th-order Linkwitz-Riley")
{
    const double fs = 48000.0;
    const double lo = 150.0, hi = 2500.0;
    // At each crossover both neighbouring bands are -6 dB; 24 dB/oct skirts.
    CHECK_NEAR (toDb (BandSplitter::bandMagnitudeAt (0, lo, lo, hi, true, fs)), -6.02, 0.05);
    CHECK_NEAR (toDb (BandSplitter::bandMagnitudeAt (1, lo, lo, hi, true, fs)), -6.02, 0.1);
    CHECK_NEAR (toDb (BandSplitter::bandMagnitudeAt (1, hi, lo, hi, true, fs)), -6.02, 0.1);
    CHECK_NEAR (toDb (BandSplitter::bandMagnitudeAt (2, hi, lo, hi, true, fs)), -6.02, 0.05);
    const double lowOctaveAbove = toDb (BandSplitter::bandMagnitudeAt (0, 2 * lo, lo, hi, true, fs));
    note ("LOW band one octave above its crossover: %.1f dB", lowOctaveAbove);
    CHECK (lowOctaveAbove < -23.0);

    // The splitter's measured output matches the model.
    BandSplitter split;
    split.prepare (fs);
    split.setCrossoversImmediate (static_cast<float> (lo), static_cast<float> (hi));
    const int n = 48000;
    for (double hz : { 60.0, 150.0, 800.0, 2500.0, 8000.0 })
    {
        auto x = sine (fs, hz, 0.25, n);
        std::vector<float> b0 (static_cast<size_t> (n)), b1 (static_cast<size_t> (n)), b2 (static_cast<size_t> (n)), unused (static_cast<size_t> (n));
        split.reset();
        const float* in[2] = { x.data(), x.data() };
        float* const out[3][2] = { { b0.data(), unused.data() }, { b1.data(), unused.data() }, { b2.data(), unused.data() } };
        split.process (in, 1, n, true, out);
        for (int b = 0; b < 3; ++b)
        {
            const auto& y = b == 0 ? b0 : b == 1 ? b1 : b2;
            const double measured = toDb (toneAmplitude (y.data() + n / 2, n / 2, fs, hz) / 0.25);
            const double model = toDb (BandSplitter::bandMagnitudeAt (b, hz, lo, hi, true, fs));
            if (model > -60.0)
                CHECK_NEAR (measured, model, 0.1);
        }
    }
}

HEAT_TEST ("Multiband", "dynamic shelves: exact at 0 dB, monotonic, midpoint at the corner")
{
    const double fs = 48000.0;
    for (bool low : { true, false })
        for (float gainDb : { -12.0f, -4.0f, 6.0f })
        {
            const double fc = low ? 150.0 : 2500.0;
            const double farIn = shelfResponseDb (low, low ? 20.0 : 18000.0, fc, gainDb, fs);
            const double farOut = shelfResponseDb (low, low ? 5000.0 : 100.0, fc, gainDb, fs);
            const double corner = shelfResponseDb (low, fc, fc, gainDb, fs);
            CHECK_NEAR (farIn, gainDb, low ? 0.35 : 0.6);
            CHECK_NEAR (farOut, 0.0, 0.3);
            CHECK_NEAR (corner, 0.5 * gainDb, 0.3);

            // Monotonic between the two plateaus (no bump at the corner): the
            // response moves one way only as frequency rises.
            bool monotonic = true, bounded = true;
            double prev = 0.0;
            for (double hz = 20.0; hz < 20000.0; hz *= 1.25)
            {
                const double m = toDb (low ? ShelfCoefficients::lowShelfMagnitude (hz, fc, gainDb, fs)
                                           : ShelfCoefficients::highShelfMagnitude (hz, fc, gainDb, fs));
                const double lo = std::min (0.0, (double) gainDb) - 0.01, hi = std::max (0.0, (double) gainDb) + 0.01;
                bounded = bounded && m >= lo && m <= hi;
                if (hz > 20.0)
                {
                    // Low shelf: moves from gainDb towards 0; high shelf: from 0 towards gainDb.
                    const double dir = (low ? -gainDb : gainDb) > 0.0 ? 1.0 : -1.0;
                    monotonic = monotonic && dir * (m - prev) >= -1.0e-6;
                }
                prev = m;
            }
            CHECK (bounded);
            CHECK (monotonic);
        }

    // 0 dB is an exact identity.
    ShelfState s;
    const auto c = ShelfCoefficients::lowShelf (static_cast<float> (std::tan (pi * 150.0 / fs)), 0.0f);
    Noise noise (2);
    double err = 0.0;
    for (int i = 0; i < 10000; ++i)
    {
        const float x = noise.next();
        err = std::max (err, (double) std::abs (s.process (x, c) - x));
    }
    CHECK (err <= 0.0);
}

HEAT_TEST ("Multiband", "neutral multiband processing is a delayed copy (and MIX stays phase-true)")
{
    const double fs = 48000.0;
    const int n = 48000;
    Noise noise (9);
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
        in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] = 0.4f * noise.next();
    for (auto mb : { MultibandMode::twoBand, MultibandMode::threeBand })
        for (float mixValue : { 1.0f, 0.5f })
        {
            auto p = neutralParams();
            p.multiband = mb;
            p.mix = mixValue;
            HeatEngine e;
            const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
            const int lat = e.getLatencySamples();
            double err = 0.0;
            for (int i = lat; i < n; ++i)
                err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
            note ("bands %.0f, MIX %.1f: null residual %.2e", mb == MultibandMode::twoBand ? 2.0 : 3.0, mixValue, err);
            CHECK (err < 1.0e-6);
        }
}

HEAT_TEST ("Multiband", "bass no longer pumps the presence range")
{
    const double fs = 48000.0;
    const int n = 96000;
    const auto in = bassAndPresence (fs, n);

    // Modulation depth of the quiet 3 kHz tone (dB swing of its envelope).
    auto presenceSwingDb = [&] (MultibandMode mb, bool colour)
    {
        auto p = neutralParams();
        p.compress = 0.75f;
        p.attackMs = 5.0f;
        p.releaseMs = 60.0f;
        p.multiband = mb;
        p.xoverLowHz = 200.0f;
        p.xoverHighHz = 2000.0f;
        if (colour)
        {
            p.mode = Mode::warm;
            p.tube = 0.1f;
        }
        const auto out = runEngine (p, in, fs);
        // Isolate 3 kHz with a narrow band-pass (SVF) and follow its envelope.
        TptSvf bp;
        bp.design (3000.0, fs, 0.05);
        double lo = 1.0e9, hi = 0.0, env = 0.0;
        for (int i = 0; i < n; ++i)
        {
            float v1, v2;
            bp.tick (out.l[static_cast<size_t> (i)], v1, v2);
            env = std::max (std::abs ((double) v1) * 0.05, env * 0.999);
            if (i > n / 2)
            {
                lo = std::min (lo, env);
                hi = std::max (hi, env);
            }
        }
        return toDb (hi / lo);
    };

    const double single = presenceSwingDb (MultibandMode::off, false);
    const double three = presenceSwingDb (MultibandMode::threeBand, false);
    const double two = presenceSwingDb (MultibandMode::twoBand, false);
    const double threeColour = presenceSwingDb (MultibandMode::threeBand, true);
    note ("3 kHz envelope swing: single band %.2f dB, 2 bands %.2f dB, 3 bands %.2f dB (3 bands + colour %.2f dB)",
          single, two, three, threeColour);
    CHECK (single > 4.0);
    CHECK (three < 1.0);
    CHECK (two < 1.5);
    CHECK (threeColour < 1.0);
}

HEAT_TEST ("Multiband", "band amounts set how hard each band works")
{
    const double fs = 48000.0;
    const int n = 48000;
    const auto in = bassAndPresence (fs, n);
    auto lowGr = [&] (float lowAmount)
    {
        auto p = neutralParams();
        p.compress = 0.7f;
        p.multiband = MultibandMode::threeBand;
        p.bandAmount[0] = lowAmount;
        HeatEngine e;
        runEngine (p, in, fs, 256, nullptr, {}, &e);
        return (double) e.getTelemetry().bandGrDb[0];
    };
    const double off = lowGr (0.0f), normal = lowGr (1.0f), hard = lowGr (1.6f);
    note ("LOW band GR at amount 0 / 100 / 160 %%: %.2f / %.2f / %.2f dB", off, normal, hard);
    CHECK (off > -0.01);
    CHECK (normal < -1.0);
    CHECK (hard < normal - 1.0);
}

HEAT_TEST ("Multiband", "switching multiband on, off and between 2 / 3 bands is click-free")
{
    const double fs = 48000.0;
    const int n = 144000;
    const auto in = bassAndPresence (fs, n);
    auto p = neutralParams();
    p.compress = 0.7f;
    for (bool colour : { false, true })
    {
        p.tube = colour ? 0.3f : 0.0f;
        p.mode = colour ? Mode::warm : Mode::clean;
        const auto out = runEngine (p, in, fs, 256, nullptr, [] (int start, EngineParams& q)
        {
            q.multiband = start < 24000 ? MultibandMode::off : start < 60000 ? MultibandMode::threeBand
                        : start < 96000 ? MultibandMode::twoBand : start < 120000 ? MultibandMode::threeBand
                        : MultibandMode::off;
            q.xoverLowHz = start > 108000 ? 400.0f : 200.0f;
        });
        double worst = 0.0;
        for (int at : { 24064, 60160, 96000, 108032, 120064 })
            worst = std::max (worst, clickRatio (out.l, at, 6000));
        bool finite = true;
        for (float v : out.l)
            finite = finite && std::isfinite (v);
        note ("colour %.0f: worst click ratio around changes %.2f (1 = steady state)", colour ? 1.0 : 0.0, worst);
        CHECK (finite);
        CHECK (worst < 1.3);
    }
}
