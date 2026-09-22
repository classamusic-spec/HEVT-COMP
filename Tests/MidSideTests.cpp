#include "TestFramework.h"
#include "EngineHarness.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    // Stereo programme with independent mid and side content.
    StereoBuffer midSideSignal (double fs, int n, double midAmp, double midHz, double sideAmp, double sideHz)
    {
        StereoBuffer b (n);
        for (int i = 0; i < n; ++i)
        {
            const double m = midAmp * std::sin (2.0 * pi * midHz * i / fs);
            const double s = sideAmp * std::sin (2.0 * pi * sideHz * i / fs);
            b.l[static_cast<size_t> (i)] = static_cast<float> (m + s);
            b.r[static_cast<size_t> (i)] = static_cast<float> (m - s);
        }
        return b;
    }

    std::vector<float> midOf (const StereoBuffer& b)
    {
        std::vector<float> m (b.l.size());
        for (size_t i = 0; i < m.size(); ++i)
            m[i] = 0.5f * (b.l[i] + b.r[i]);
        return m;
    }

    std::vector<float> sideOf (const StereoBuffer& b)
    {
        std::vector<float> s (b.l.size());
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = 0.5f * (b.l[i] - b.r[i]);
        return s;
    }
}

HEAT_TEST ("MidSide", "rotation is exactly invertible for every morph position")
{
    Noise noise (5);
    double worst = 0.0;
    for (float t : { 0.0f, 0.1f, 0.37f, 0.5f, 0.83f, 1.0f })
    {
        const auto m = HeatEngine::StereoMatrix::forMorph (t);
        for (int i = 0; i < 1000; ++i)
        {
            const float l = noise.next(), r = noise.next();
            float a, b, l2, r2;
            m.encode (l, r, a, b);
            m.decode (a, b, l2, r2);
            worst = std::max ({ worst, (double) std::abs (l2 - l), (double) std::abs (r2 - r) });
        }
    }
    const auto ms = HeatEngine::StereoMatrix::forMorph (1.0f);
    float a, b;
    ms.encode (0.8f, 0.2f, a, b);
    note ("round-trip error %.2e; M/S of (0.8, 0.2) = (%.3f, %.3f)", worst, a, b);
    CHECK (worst < 1.0e-6);
    CHECK_NEAR (a, 0.5, 1.0e-7);   // (L + R) / 2
    CHECK_NEAR (b, -0.3, 1.0e-7);  // (R − L) / 2
}

HEAT_TEST ("MidSide", "neutral M/S processing is a delayed copy")
{
    const double fs = 48000.0;
    const int n = 24000;
    Noise noise (8);
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
    {
        in.l[static_cast<size_t> (i)] = 0.5f * noise.next();
        in.r[static_cast<size_t> (i)] = 0.5f * noise.next();
    }
    for (auto mode : { StereoMode::midSide, StereoMode::midOnly, StereoMode::sideOnly })
    {
        auto p = neutralParams();
        p.stereoMode = mode;
        HeatEngine e;
        const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
        const int lat = e.getLatencySamples();
        double err = 0.0;
        for (int i = lat; i < n; ++i)
            err = std::max ({ err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]),
                              (double) std::abs (out.r[static_cast<size_t> (i)] - in.r[static_cast<size_t> (i - lat)]) });
        note ("stereo mode %.0f: null residual %.2e", (double) mode, err);
        CHECK (err < 1.0e-6);
    }
}

HEAT_TEST ("MidSide", "mono material: M/S equals L/R")
{
    // L = R → M = L, S = 0: the linked detector sees the same level, so the
    // result must match L/R processing.
    const double fs = 48000.0;
    const int n = 48000;
    auto in = stereoSine (fs, 440.0, 0.5, 0.5, n);
    auto p = neutralParams();
    p.compress = 0.7f;
    p.mode = Mode::warm;
    p.tube = 0.3f;
    const auto lr = runEngine (p, in, fs);
    p.stereoMode = StereoMode::midSide;
    const auto ms = runEngine (p, in, fs);
    double err = 0.0;
    for (int i = 0; i < n; ++i)
        err = std::max (err, (double) std::abs (lr.l[static_cast<size_t> (i)] - ms.l[static_cast<size_t> (i)]));
    note ("mono programme, L/R vs M/S max difference %.2e", err);
    CHECK (err < 1.0e-4);
}

HEAT_TEST ("MidSide", "M/S dual mono compresses mid and side independently")
{
    // Loud mid, quiet side (below threshold): the side must come through at
    // its own level.
    const double fs = 48000.0;
    const int n = 48000;
    const auto in = midSideSignal (fs, n, 0.7, 200.0, 0.01, 1500.0);
    auto p = neutralParams();
    p.compress = 0.8f;
    p.autoMakeup = false;
    p.link = StereoLink::dualMono;

    p.stereoMode = StereoMode::leftRight;
    const auto lr = runEngine (p, in, fs);
    p.stereoMode = StereoMode::midSide;
    const auto ms = runEngine (p, in, fs);

    const int a = n / 2, len = n / 2;
    auto gainDb = [&] (const std::vector<float>& x, double amp, double hz) { return toDb (toneAmplitude (x.data() + a, len, fs, hz) / amp); };
    const double lrMid = gainDb (midOf (lr), 0.7, 200.0), lrSide = gainDb (sideOf (lr), 0.01, 1500.0);
    const double msMid = gainDb (midOf (ms), 0.7, 200.0), msSide = gainDb (sideOf (ms), 0.01, 1500.0);
    note ("L/R dual mono: mid %.2f dB, side %.2f dB", lrMid, lrSide);
    note ("M/S dual mono: mid %.2f dB, side %.2f dB", msMid, msSide);
    CHECK (msMid < -6.0);           // the loud mid is compressed
    CHECK (msSide > -0.3);          // the quiet side is left alone
    CHECK (lrSide < msSide - 10.0); // in L/R the side is dragged down with the mid
}

HEAT_TEST ("MidSide", "MID only leaves the side untouched (and vice versa)")
{
    const double fs = 48000.0;
    const int n = 48000;
    const auto in = midSideSignal (fs, n, 0.6, 150.0, 0.4, 900.0);
    for (auto mode : { StereoMode::midOnly, StereoMode::sideOnly })
    {
        auto p = neutralParams();
        p.compress = 0.9f;
        p.mode = Mode::drive;
        p.tube = 0.6f;
        p.iron = 0.5f;
        p.stereoMode = mode;
        HeatEngine e;
        const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
        const int lat = e.getLatencySamples();
        const auto inMid = midOf (in), inSide = sideOf (in), outMid = midOf (out), outSide = sideOf (out);
        const auto& untouchedIn = mode == StereoMode::midOnly ? inSide : inMid;
        const auto& untouchedOut = mode == StereoMode::midOnly ? outSide : outMid;
        const auto& processedIn = mode == StereoMode::midOnly ? inMid : inSide;
        const auto& processedOut = mode == StereoMode::midOnly ? outMid : outSide;
        double err = 0.0;
        for (int i = n / 2; i < n; ++i)
            err = std::max (err, (double) std::abs (untouchedOut[static_cast<size_t> (i)] - untouchedIn[static_cast<size_t> (i - lat)]));
        const double change = toDb (rmsOf (processedOut.data() + n / 2, n / 2) / rmsOf (processedIn.data() + n / 2, n / 2));
        note ("mode %.0f (2 = MID only, 3 = SIDE only): untouched channel error %.2e, processed channel %+.2f dB",
              (double) mode, err, change);
        CHECK (err < 1.0e-5);
        CHECK (change < -3.0);
    }
}

HEAT_TEST ("MidSide", "switching stereo mode mid-stream is click-free")
{
    const double fs = 48000.0;
    const int n = 96000;
    const auto in = midSideSignal (fs, n, 0.5, 180.0, 0.2, 700.0);
    auto p = neutralParams();
    p.compress = 0.6f;
    p.mode = Mode::warm;
    p.tube = 0.3f;
    const auto out = runEngine (p, in, fs, 256, nullptr, [] (int start, EngineParams& q)
    {
        q.stereoMode = start < 24000 ? StereoMode::leftRight : start < 48000 ? StereoMode::midSide
                     : start < 72000 ? StereoMode::midOnly : StereoMode::sideOnly;
    });
    double worst = 0.0;
    for (int at : { 24064, 48128, 72192 })
        worst = std::max ({ worst, clickRatio (out.l, at, 4000), clickRatio (out.r, at, 4000) });
    note ("worst click ratio around stereo-mode changes %.2f (1 = steady state)", worst);
    CHECK (worst < 1.3);
}
