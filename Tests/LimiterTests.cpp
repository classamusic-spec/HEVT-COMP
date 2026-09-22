#include "TestFramework.h"
#include "EngineHarness.h"
#include "DSP/TruePeakLimiter.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    // Independent reference true-peak meter: 16x windowed-sinc interpolation,
    // 128 taps per phase, Kaiser β = 9 (far longer than the limiter's own
    // 16-tap detector).
    double truePeak (const std::vector<float>& x, int start, int end)
    {
        const int half = 64, factor = 16;
        auto i0 = [] (double v) { double s = 1.0, t = 1.0; for (int k = 1; k < 50; ++k) { t *= (v / (2.0 * k)) * (v / (2.0 * k)); s += t; } return s; };
        std::vector<double> taps (static_cast<size_t> (factor * 2 * half));
        for (int p = 1; p < factor; ++p)
            for (int j = -half + 1; j <= half; ++j)
            {
                const double d = j - static_cast<double> (p) / factor;
                const double r = d / (half + 1);
                taps[static_cast<size_t> (p * 2 * half + j + half - 1)] = std::sin (pi * d) / (pi * d) * i0 (9.0 * std::sqrt (1.0 - r * r)) / i0 (9.0);
            }
        double peak = 0.0;
        for (int i = std::max (start, half); i < std::min (end, static_cast<int> (x.size()) - half); ++i)
        {
            peak = std::max (peak, (double) std::abs (x[static_cast<size_t> (i)]));
            for (int p = 1; p < factor; ++p)
            {
                double acc = 0.0;
                for (int j = -half + 1; j <= half; ++j)
                    acc += x[static_cast<size_t> (i + j)] * taps[static_cast<size_t> (p * 2 * half + j + half - 1)];
                peak = std::max (peak, std::abs (acc));
            }
        }
        return peak;
    }

    StereoBuffer drumLoop (double fs, int n, double gain)
    {
        // Sharp decaying hits + bass: hard for limiters (fast transients and
        // inter-sample overs from the band-limited clicks).
        StereoBuffer b (n);
        Noise noise (21);
        for (int i = 0; i < n; ++i)
        {
            const int k = i % 9000;
            const double env = std::exp (-k / (0.004 * fs));
            const double bass = 0.5 * std::sin (2.0 * pi * 55.0 * i / fs);
            const double hit = env * (0.9 * std::sin (2.0 * pi * 180.0 * k / fs) + 0.4 * noise.next());
            b.l[static_cast<size_t> (i)] = static_cast<float> (gain * (bass + hit));
            b.r[static_cast<size_t> (i)] = static_cast<float> (gain * (0.8 * bass + hit));
        }
        return b;
    }
}

HEAT_TEST ("Limiter", "sample peaks never exceed the ceiling")
{
    const double fs = 48000.0;
    const int n = 96000;
    for (float ceilingDb : { -1.0f, -6.0f, 0.0f })
        for (double gain : { 1.0, 2.5, 6.0 })
        {
            auto p = neutralParams();
            p.limiter = true;
            p.ceilingDb = ceilingDb;
            const auto out = runEngine (p, drumLoop (fs, n, gain), fs);
            const double peak = std::max (peakOf (out.l.data() + 4800, n - 4800), peakOf (out.r.data() + 4800, n - 4800));
            const double ceiling = std::pow (10.0, ceilingDb / 20.0);
            if (peak > ceiling * (1.0 + 1.0e-6))
                note ("ceiling %.1f dB, drive x%.1f: sample peak %.4f dB", ceilingDb, gain, toDb (peak));
            CHECK (peak <= ceiling * (1.0 + 1.0e-6));
        }

    // Square-ish and full-scale sines driven far over the ceiling.
    for (double hz : { 60.0, 997.0, 9000.0, 17000.0 })
    {
        auto p = neutralParams();
        p.limiter = true;
        p.ceilingDb = -1.0f;
        p.inputDb = 12.0f;
        const auto out = runEngine (p, stereoSine (fs, hz, 0.9, 0.9, 48000), fs);
        const double peak = peakOf (out.l.data() + 4800, 48000 - 4800);
        CHECK (peak <= std::pow (10.0, -1.0 / 20.0) * (1.0 + 1.0e-6));
    }
}

HEAT_TEST ("Limiter", "true peak stays at the ceiling (independent 16x meter)")
{
    const double fs = 48000.0;
    const int n = 96000;
    double worstOverDb = -100.0;
    for (double gain : { 1.5, 4.0 })
    {
        auto p = neutralParams();
        p.limiter = true;
        p.ceilingDb = -1.0f;
        const auto out = runEngine (p, drumLoop (fs, n, gain), fs);
        worstOverDb = std::max (worstOverDb, toDb (truePeak (out.l, 4800, n)) + 1.0);
    }
    // Worst case for inter-sample peaks: a sine at fs/4 with a 45° phase
    // (samples at ±0.707 of the true peak).
    {
        StereoBuffer b (48000);
        for (int i = 0; i < 48000; ++i)
            b.l[static_cast<size_t> (i)] = b.r[static_cast<size_t> (i)] = static_cast<float> (1.2 * std::sin (0.5 * pi * i + 0.25 * pi));
        auto p = neutralParams();
        p.limiter = true;
        p.ceilingDb = -1.0f;
        const auto out = runEngine (p, b, fs);
        const double samplePeak = toDb (peakOf (out.l.data() + 4800, 40000));
        const double tp = toDb (truePeak (out.l, 4800, 44000));
        note ("fs/4 sine at +1.6 dBFS true peak: output sample peak %.2f dBFS, true peak %.2f dBTP", samplePeak, tp);
        worstOverDb = std::max (worstOverDb, tp + 1.0);
        CHECK (samplePeak < -3.0); // the limiter saw the inter-sample peak
    }
    note ("worst true-peak overshoot above a -1 dBTP ceiling: %+.3f dB", worstOverDb);
    CHECK (worstOverDb < 0.2);
}

HEAT_TEST ("Limiter", "below the ceiling the limiter is an exact (delayed) bypass")
{
    const double fs = 48000.0;
    const int n = 48000;
    Noise noise (4);
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
    {
        in.l[static_cast<size_t> (i)] = 0.3f * noise.next();
        in.r[static_cast<size_t> (i)] = 0.3f * noise.next();
    }
    auto p = neutralParams();
    p.limiter = true;
    p.ceilingDb = -1.0f;
    HeatEngine e;
    const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
    const int lat = e.getLatencySamples();
    double err = 0.0;
    for (int i = lat; i < n; ++i)
        err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
    note ("-10 dBFS noise through the limiter: null residual %.2e", err);
    CHECK (err <= 0.0);
}

HEAT_TEST ("Limiter", "release recovers smoothly with the documented time constant")
{
    // A short burst over the ceiling, then a steady tone below it: after the
    // burst, the gain returns to 1 following the release time constant.
    const double fs = 48000.0;
    TruePeakLimiter lim;
    lim.prepare (fs);
    lim.setCeilingDb (-6.0f);
    std::vector<float> g;
    for (int i = 0; i < 48000; ++i)
    {
        const float x = (i >= 1000 && i < 1480 ? 1.0f : 0.1f) * static_cast<float> (std::sin (2.0 * pi * 1000.0 * i / fs));
        const float frame[2] = { x, x };
        g.push_back (lim.process (frame, 2));
    }
    float minG = 1.0f;
    int minAt = 0;
    for (int i = 0; i < static_cast<int> (g.size()); ++i)
        if (g[static_cast<size_t> (i)] < minG)
        {
            minG = g[static_cast<size_t> (i)];
            minAt = i;
        }
    // Time from the end of the hold until the remaining reduction has fallen to 1/e.
    const double target = 1.0 - (1.0 - minG) / std::exp (1.0);
    int recovered = minAt;
    while (recovered < static_cast<int> (g.size()) && g[static_cast<size_t> (recovered)] < target)
        ++recovered;
    const double tauMs = (recovered - minAt) * 1000.0 / fs;
    note ("minimum gain %.2f dB, 1/e recovery %.1f ms (release constant %.0f ms + %.1f ms window)",
          toDb (minG), tauMs, TruePeakLimiter::releaseMs, TruePeakLimiter::attackMs);
    CHECK_RANGE (tauMs, TruePeakLimiter::releaseMs * 0.85, TruePeakLimiter::releaseMs * 1.25 + 2.0 * TruePeakLimiter::attackMs);
    CHECK (g.back() >= 1.0f);

    // Smooth: the gain never steps by more than the box filter allows.
    double maxStepDb = 0.0;
    for (size_t i = 1; i < g.size(); ++i)
        maxStepDb = std::max (maxStepDb, std::abs (toDb (g[i]) - toDb (g[i - 1])));
    note ("largest per-sample gain step %.3f dB", maxStepDb);
    CHECK (maxStepDb < 0.5);
}

HEAT_TEST ("Limiter", "stereo gain is linked")
{
    const double fs = 48000.0;
    const int n = 48000;
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
    {
        in.l[static_cast<size_t> (i)] = static_cast<float> (1.5 * std::sin (2.0 * pi * 200.0 * i / fs)); // over
        in.r[static_cast<size_t> (i)] = static_cast<float> (0.2 * std::sin (2.0 * pi * 300.0 * i / fs)); // quiet
    }
    auto p = neutralParams();
    p.limiter = true;
    p.ceilingDb = -1.0f;
    const auto out = runEngine (p, in, fs);
    const double rGain = toDb (toneAmplitude (out.r.data() + n / 2, n / 2, fs, 300.0) / 0.2);
    const double lGain = toDb (toneAmplitude (out.l.data() + n / 2, n / 2, fs, 200.0) / 1.5);
    note ("left (over) %.2f dB, right (quiet) %.2f dB", lGain, rGain);
    CHECK_NEAR (rGain, lGain, 0.3);
}
