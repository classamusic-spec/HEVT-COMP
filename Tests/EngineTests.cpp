#include "TestFramework.h"
#include "EngineHarness.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    StereoBuffer programme (double fs, int n, uint32_t seed = 3)
    {
        // Drum-and-bass-like test programme: decaying noise hits, a bass line
        // and a sustained mid tone with slow level movement.
        StereoBuffer b (n);
        Noise noise (seed);
        for (int i = 0; i < n; ++i)
        {
            const int t = i % static_cast<int> (fs * 0.25);
            const double hit = std::exp (-t / (0.004 * fs)) * noise.next();
            const double bass = 0.3 * std::sin (2 * pi * 55 * i / fs);
            const double mid = 0.15 * (1.0 + 0.5 * std::sin (2 * pi * 0.5 * i / fs)) * std::sin (2 * pi * 660 * i / fs);
            const float v = static_cast<float> (0.6 * hit + bass + mid);
            b.l[static_cast<size_t> (i)] = v;
            b.r[static_cast<size_t> (i)] = 0.9f * v;
        }
        return b;
    }
}

HEAT_TEST ("Engine", "every mode / detector / quality combination is finite and bounded")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 0.5);
    const auto in = programme (fs, n);
    for (int m = 0; m < 3; ++m)
        for (int d = 0; d < 3; ++d)
            for (int q = 0; q < 3; ++q)
            {
                EngineParams p;
                p.mode = static_cast<Mode> (m);
                p.detector = static_cast<Detector> (d);
                p.quality = static_cast<Quality> (q);
                p.compress = 1.0f;
                p.inputDb = 24.0f;
                p.tube = 1.0f;
                p.iron = 1.0f;
                p.outputDb = 24.0f;
                const auto out = runEngine (p, in, fs);
                bool finite = true;
                for (int i = 0; i < n; ++i)
                    finite = finite && std::isfinite (out.l[static_cast<size_t> (i)]) && std::isfinite (out.r[static_cast<size_t> (i)]);
                CHECK (finite);
                CHECK (peakOf (out.l.data(), n) <= 31.7);
            }
}

HEAT_TEST ("Engine", "mode switching is click-free")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 2);
    const auto in = stereoSine (fs, 220.0, 0.4, 0.4, n);

    EngineParams base;
    base.compress = 0.6f;
    base.detector = Detector::rms;
    // Reference: no switching.
    const auto steady = runEngine (base, in, fs);
    const double refJump = maxSecondDifference (steady.l, n / 4, n);

    const auto switched = runEngine (base, in, fs, 256, nullptr, [&] (int start, EngineParams& p)
    {
        p.mode = static_cast<Mode> ((start / 4800) % 3); // switch every 100 ms
    });
    const double jump = maxSecondDifference (switched.l, n / 4, n);
    note ("second difference: steady %.5f, switching modes every 100 ms %.5f", refJump, jump);
    CHECK (jump < 3.0 * refJump + 1.0e-3);
}

HEAT_TEST ("Engine", "detector switching is click-free")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 2);
    const auto in = stereoSine (fs, 150.0, 0.5, 0.5, n);
    EngineParams base;
    base.mode = Mode::clean;
    base.compress = 0.7f;
    const auto steady = runEngine (base, in, fs);
    const double refJump = maxSecondDifference (steady.l, n / 4, n);
    const auto switched = runEngine (base, in, fs, 256, nullptr, [&] (int start, EngineParams& p)
    {
        p.detector = static_cast<Detector> ((start / 3840) % 3);
    });
    const double jump = maxSecondDifference (switched.l, n / 4, n);
    note ("second difference: steady %.5f, switching detectors %.5f", refJump, jump);
    CHECK (jump < 3.0 * refJump + 1.0e-3);
}

HEAT_TEST ("Engine", "TUBE / IRON / QUALITY changes are click-free")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 3);
    const auto in = stereoSine (fs, 330.0, 0.3, 0.3, n);
    EngineParams base;
    base.mode = Mode::clean;
    base.compress = 0.3f;
    const auto steady = runEngine (base, in, fs);
    const double refJump = maxSecondDifference (steady.l, n / 6, n);
    const auto switched = runEngine (base, in, fs, 256, nullptr, [&] (int start, EngineParams& p)
    {
        const int phase = start / 9600; // every 200 ms
        p.tube = (phase % 2) ? 0.8f : 0.0f;
        p.iron = (phase % 3 == 1) ? 0.7f : 0.0f;
        p.quality = static_cast<Quality> (phase % 3);
    });
    const double jump = maxSecondDifference (switched.l, n / 6, n);
    note ("second difference: steady %.5f, toggling tube/iron/quality %.5f", refJump, jump);
    CHECK (jump < 4.0 * refJump + 2.0e-3);
}

HEAT_TEST ("Engine", "bypass is click-free and exact")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 2);
    const auto in = stereoSine (fs, 440.0, 0.5, 0.5, n);
    EngineParams base;
    base.compress = 0.8f;
    base.tube = 0.5f;
    HeatEngine e;
    const auto out = runEngine (base, in, fs, 256, nullptr, [&] (int start, EngineParams& p)
    {
        p.bypass = start >= n / 2;
    }, &e);
    const int lat = e.getLatencySamples();
    double err = 0.0;
    for (int i = n / 2 + 4800; i < n; ++i)
        err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
    const double jump = maxSecondDifference (out.l, n / 4, n);
    const double ref = maxSecondDifference (in.l, 0, n);
    note ("bypassed residual %.2e, max 2nd diff %.4f (input %.4f)", err, jump, ref);
    CHECK (err < 1.0e-6);
    CHECK (jump < 3.0 * ref);
}

HEAT_TEST ("Engine", "output independent of host block size")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs);
    const auto in = programme (fs, n);
    EngineParams p;
    p.compress = 0.7f;
    p.tube = 0.4f;
    p.iron = 0.3f;
    p.mode = Mode::drive;
    const auto ref = runEngine (p, in, fs, 512);
    double worst = 0.0;
    for (int bs : { 16, 32, 64, 100, 128, 256, 1024, 2048, 4096 })
    {
        const auto out = runEngine (p, in, fs, bs);
        double err = 0.0;
        for (int i = 0; i < n; ++i)
            err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - ref.l[static_cast<size_t> (i)]));
        worst = std::max (worst, err);
    }
    note ("max deviation across block sizes 16..4096: %.2e", worst);
    CHECK (worst < 1.0e-4);
}

HEAT_TEST ("Engine", "gain reduction consistent across sample rates")
{
    double grAt48 = 0.0;
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        const int n = static_cast<int> (fs * 1.5);
        auto in = stereoSine (fs, 1000.0, 0.25, 0.25, n);
        EngineParams p;
        p.mode = Mode::clean;
        p.compress = 0.5f;
        HeatEngine e;
        runEngine (p, in, fs, 512, nullptr, {}, &e);
        const double gr = e.getTelemetry().grDbLast[0];
        if (fs == 48000.0)
            grAt48 = gr;
        note ("fs %.0f: GR %.3f dB, latency %.0f samples, OS x%.0f", fs, gr, e.getLatencySamples(), e.getTelemetry().oversamplingFactor);
        if (grAt48 != 0.0)
            CHECK_NEAR (gr, grAt48, 0.05);
    }
}

HEAT_TEST ("Engine", "OPTICAL: program-dependent two-stage release")
{
    const double fs = 48000.0;
    auto recoveryAfter = [&] (double holdSeconds, Detector det)
    {
        const int hold = static_cast<int> (fs * holdSeconds);
        const int n = hold + static_cast<int> (fs * 4);
        StereoBuffer in (n);
        for (int i = 0; i < hold; ++i)
            in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] =
                static_cast<float> (0.5 * std::sin (2 * pi * 500 * i / fs));
        for (int i = hold; i < n; ++i)
            in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] =
                static_cast<float> (0.003 * std::sin (2 * pi * 500 * i / fs));

        EngineParams p;
        p.mode = Mode::clean;
        p.detector = det;
        p.compress = 0.7f;
        p.releaseMs = 150.0f;
        HeatEngine e;
        double t90 = -1.0;
        float deepest = 0.0f;
        runEngine (p, in, fs, 64, nullptr, [&] (int start, EngineParams&)
        {
            const float g = e.getTelemetry().grDbLast[0];
            if (start <= hold)
                deepest = std::min (deepest, g);
            else if (t90 < 0.0 && g > 0.1f * deepest)
                t90 = (start - hold) * 1000.0 / fs;
        }, &e);
        return t90;
    };

    const double optShort = recoveryAfter (0.05, Detector::optical);
    const double optLong = recoveryAfter (3.0, Detector::optical);
    const double peakShort = recoveryAfter (0.05, Detector::peak);
    const double peakLong = recoveryAfter (3.0, Detector::peak);
    note ("90%% recovery — OPTICAL: after 50 ms burst %.0f ms, after 3 s %.0f ms", optShort, optLong);
    note ("90%% recovery — PEAK:    after 50 ms burst %.0f ms, after 3 s %.0f ms", peakShort, peakLong);
    CHECK (optLong > 2.0 * optShort);                // memory: sustained → slower
    CHECK (std::abs (peakLong - peakShort) < 0.2 * peakShort + 10.0); // peak has no memory
}

HEAT_TEST ("Engine", "CLEAN / WARM / DRIVE are measurably distinct")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 2);
    const auto in = programme (fs, n);
    double meanGr[3], thdAt[3];
    for (int m = 0; m < 3; ++m)
    {
        EngineParams p;
        p.mode = static_cast<Mode> (m);
        p.compress = 0.6f;
        HeatEngine e;
        double sum = 0.0;
        int count = 0;
        runEngine (p, in, fs, 256, nullptr, [&] (int start, EngineParams&)
        {
            if (start > n / 2)
            {
                sum += e.getTelemetry().grDbLast[0];
                ++count;
            }
        }, &e);
        meanGr[m] = sum / std::max (1, count);

        // Harmonic character on a steady -12 dBFS 200 Hz tone (compressor active).
        const int nt = 65536;
        auto tone = stereoSine (fs, 187.5, 0.25, 0.25, nt);
        const auto out = runEngine (p, tone, fs);
        thdAt[m] = thd (out.l.data() + nt / 2, nt / 2, fs, 187.5, 9);
    }
    note ("mean GR: CLEAN %.2f  WARM %.2f  DRIVE %.2f dB", meanGr[0], meanGr[1], meanGr[2]);
    note ("THD @ -12 dBFS 187.5 Hz: CLEAN %.4f%%  WARM %.4f%%  DRIVE %.4f%%", thdAt[0] * 100, thdAt[1] * 100, thdAt[2] * 100);
    CHECK (std::abs (meanGr[0] - meanGr[1]) > 0.3 || std::abs (thdAt[0] - thdAt[1]) > 0.001);
    CHECK (thdAt[2] > 2.0 * thdAt[1]);
    CHECK (thdAt[1] > 2.0 * thdAt[0]);
}
