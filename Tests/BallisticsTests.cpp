#include "TestFramework.h"
#include "TestSignals.h"
#include "CompressorHarness.h"
#include "DSP/Ballistics.h"
#include "DSP/ControlMappings.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    // Time (ms) for a step response to cover 63.2 % of its travel.
    double timeTo63 (Ballistics& b, float from, float to, double fs)
    {
        b.reset (from);
        const float target = from + 0.632f * (to - from);
        for (int i = 0; i < static_cast<int> (fs * 20); ++i)
        {
            const float y = b.process (to);
            if ((to < from && y <= target) || (to > from && y >= target))
                return (i + 1) * 1000.0 / fs;
        }
        return -1.0;
    }
}

HEAT_TEST ("Ballistics", "attack and release follow requested time constants")
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (float attack : { 0.1f, 1.0f, 10.0f, 100.0f })
        {
            Ballistics b;
            b.prepare (fs);
            BallisticsSettings s;
            s.attackMs = attack;
            s.releaseMs = 200.0f;
            b.setSettings (s);
            const double t = timeTo63 (b, 0.0f, -12.0f, fs);
            const double tolerance = std::max (0.05 * attack, 1500.0 / fs);
            CHECK_NEAR (t, attack, tolerance);
        }

        for (float release : { 20.0f, 100.0f, 1000.0f, 3000.0f })
        {
            Ballistics b;
            b.prepare (fs);
            BallisticsSettings s;
            s.attackMs = 1.0f;
            s.releaseMs = release;
            b.setSettings (s);
            const double t = timeTo63 (b, -12.0f, 0.0f, fs);
            CHECK_NEAR (t, release, 0.005 * release + 1000.0 / fs);
        }
    }
}

HEAT_TEST ("Ballistics", "timing is sample-rate independent")
{
    double t44 = 0.0, t192 = 0.0;
    for (double fs : { 44100.0, 192000.0 })
    {
        Ballistics b;
        b.prepare (fs);
        BallisticsSettings s;
        s.attackMs = 5.0f;
        s.releaseMs = 300.0f;
        s.roundingMs = 1.0f;
        s.memoryWeight = 0.8f;
        b.setSettings (s);
        const double t = timeTo63 (b, -10.0f, 0.0f, fs);
        (fs < 50000 ? t44 : t192) = t;
    }
    note ("release 63%% at 44.1k: %.2f ms, at 192k: %.2f ms", t44, t192);
    CHECK_NEAR (t44, t192, 0.001 * t44 + 0.05);
}

HEAT_TEST ("Ballistics", "program dependence: sustained compression recovers slower")
{
    const double fs = 48000.0;
    auto recoveryMs = [fs] (int holdSamples)
    {
        Ballistics b;
        b.prepare (fs);
        BallisticsSettings s;
        s.attackMs = 1.0f;
        s.releaseMs = 80.0f;
        s.memoryWeight = 1.0f;
        s.memoryDepth = 0.6f;
        s.memoryChargeMs = 400.0f;
        s.memoryReleaseMs = 1200.0f;
        b.setSettings (s);
        for (int i = 0; i < holdSamples; ++i)
            b.process (-10.0f);
        for (int i = 0; i < static_cast<int> (fs * 10); ++i)
            if (b.process (0.0f) > -1.0f)
                return i * 1000.0 / fs;
        return -1.0;
    };

    const double shortHit = recoveryMs (static_cast<int> (fs * 0.02));
    const double sustained = recoveryMs (static_cast<int> (fs * 3.0));
    note ("recovery to -1 dB: after 20 ms hit %.0f ms, after 3 s sustain %.0f ms", shortHit, sustained);
    CHECK (shortHit > 0.0 && sustained > 0.0);
    CHECK (sustained > 3.0 * shortHit);
}

HEAT_TEST ("Ballistics", "rounding pole softens the attack onset")
{
    const double fs = 48000.0;
    Ballistics sharp, round;
    sharp.prepare (fs);
    round.prepare (fs);
    BallisticsSettings s;
    s.attackMs = 2.0f;
    sharp.setSettings (s);
    s.roundingMs = 1.0f;
    round.setSettings (s);

    // Initial slope (first 0.1 ms) must be gentler with rounding.
    float ys = 0.0f, yr = 0.0f;
    for (int i = 0; i < 5; ++i)
    {
        ys = sharp.process (-10.0f);
        yr = round.process (-10.0f);
    }
    note ("after 0.1 ms: sharp %.3f dB, rounded %.3f dB", ys, yr);
    CHECK (yr > ys);
}

HEAT_TEST ("Ballistics", "front-panel attack/release measured through the engine")
{
    // Level step on a 1 kHz sine (-30 → -6 dBFS) into CLEAN/PEAK, measure the
    // actual GR envelope at FAST / MID / SLOW panel positions.
    const double fs = 48000.0;
    for (float pos : { 0.0f, 0.5f, 1.0f })
    {
        const float attack = heat::dsp::attackFromNormalised (pos);
        const float release = heat::dsp::releaseFromNormalised (pos);

        CompressorEngine::Controls c;
        c.mode = Mode::clean;
        c.detector = Detector::peak;
        c.compress = 0.6f;
        c.attackMs = attack;
        c.releaseMs = release;

        const int n = static_cast<int> (fs * 12.0);
        std::vector<float> x (static_cast<size_t> (n));
        const int stepUp = static_cast<int> (fs * 1.0), stepDown = static_cast<int> (fs * 5.0);
        for (int i = 0; i < n; ++i)
        {
            const double amp = (i >= stepUp && i < stepDown) ? 0.5 : 0.0316;
            x[static_cast<size_t> (i)] = static_cast<float> (amp * std::sin (2 * pi * 1000.0 * i / fs));
        }
        const auto run = runCompressor (c, x, nullptr, fs);

        const float grHigh = run.grL[static_cast<size_t> (stepDown - 1)];
        const float grLow = run.grL[static_cast<size_t> (stepUp - 1)];
        int attackSamples = -1, releaseSamples = -1;
        for (int i = stepUp; i < stepDown; ++i)
            if (run.grL[static_cast<size_t> (i)] <= grLow + 0.632f * (grHigh - grLow)) { attackSamples = i - stepUp; break; }
        for (int i = stepDown; i < n; ++i)
            if (run.grL[static_cast<size_t> (i)] >= grHigh + 0.632f * (grLow - grHigh)) { releaseSamples = i - stepDown; break; }

        note ("panel %.2f: attack %.2f ms → measured %.2f ms", pos, attack, attackSamples * 1000.0 / fs);
        note ("panel %.2f: release %.0f ms → measured %.0f ms", pos, release, releaseSamples * 1000.0 / fs);
        CHECK (attackSamples >= 0 && releaseSamples >= 0);
        // A 1 kHz peak detector cannot resolve faster than about half a cycle.
        CHECK_NEAR (attackSamples * 1000.0 / fs, attack, std::max (0.6, 0.1 * attack));
        CHECK_NEAR (releaseSamples * 1000.0 / fs, release, 0.05 * release);
    }
}
