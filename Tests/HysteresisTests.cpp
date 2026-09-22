#include "TestFramework.h"
#include "EngineHarness.h"
#include "StageHarness.h"
#include "Nonlinear/Hysteresis.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    constexpr double fs = 48000.0;
    constexpr int fftN = 65536;

    std::vector<float> tone (double hz, double dbfs, int n = fftN * 2)
    {
        return sine (fs, hz, std::pow (10.0, dbfs / 20.0), n);
    }

    double thdOf (const std::vector<float>& y, double f0)
    {
        return thd (y.data() + y.size() - static_cast<size_t> (fftN), fftN, fs, f0, 12);
    }

    double harmonicDb (const std::vector<float>& y, double f0, int h)
    {
        const float* x = y.data() + y.size() - static_cast<size_t> (fftN);
        return toDb (toneAmplitude (x, fftN, fs, f0 * h) / std::max (1.0e-12, toneAmplitude (x, fftN, fs, f0)));
    }

    std::vector<float> hysteresis (float amount, const std::vector<float>& in, int factor = 4)
    {
        return runStage (StageKind::iron, amount, in, fs, factor, IronModel::hysteresis);
    }
}

HEAT_TEST ("Hysteresis", "the core traces a real, symmetric B-H loop")
{
    JilesAthertonCore core;
    core.reset();
    const auto& p = core.getParams();
    const int per = 4000;
    double area = 0.0, hAtZeroUp = 0.0, hAtZeroDown = 0.0, mAtZeroH = 0.0, maxM = 0.0;
    double bPrev = 0.0, hPrev = 0.0;
    for (int c = 0; c < 6; ++c)
        for (int i = 0; i < per; ++i)
        {
            const double b = 1.5 * std::sin (2.0 * pi * (c * per + i) / per);
            const double h = core.process (b);
            if (c == 5)
            {
                area += 0.5 * (h + hPrev) * (b - bPrev);           // ∮ H dB (energy loss per cycle)
                if (bPrev < 0.0 && b >= 0.0) hAtZeroUp = h;        // coercive field, rising branch
                if (bPrev > 0.0 && b <= 0.0) hAtZeroDown = h;      // falling branch
                if (hPrev > 0.0 && h <= 0.0) mAtZeroH = core.getM(); // remanence
                maxM = std::max (maxM, std::abs (core.getM()));
            }
            bPrev = b;
            hPrev = h;
        }
    note ("loop area %.4f, coercive field %.4f / %+.4f, remanence %.3f", area, hAtZeroDown, hAtZeroUp, mAtZeroH);
    note ("|M| max %.3f", maxM);
    CHECK (area > 1.0e-3);                         // a loop, not a curve
    CHECK (hAtZeroDown < 0.0 && hAtZeroUp > 0.0);  // B lags H: falling branch reaches B = 0 only at −Hc
    CHECK_NEAR (hAtZeroDown, -hAtZeroUp, 0.02 * std::abs (hAtZeroUp) + 1.0e-6); // symmetric
    CHECK (std::abs (mAtZeroH) > 0.05);            // remanent magnetisation
    CHECK (maxM <= p.ms + 1.0e-9);                 // never beyond saturation

    // Larger excitation → wider loop.
    JilesAthertonCore small;
    small.reset();
    double smallArea = 0.0;
    bPrev = hPrev = 0.0;
    for (int c = 0; c < 6; ++c)
        for (int i = 0; i < per; ++i)
        {
            const double b = 0.3 * std::sin (2.0 * pi * (c * per + i) / per);
            const double h = small.process (b);
            if (c == 5)
                smallArea += 0.5 * (h + hPrev) * (b - bPrev);
            bPrev = b;
            hPrev = h;
        }
    CHECK (smallArea > 0.0 && smallArea < 0.5 * area);
}

HEAT_TEST ("Hysteresis", "IRON (hysteresis): Rayleigh region, saturation knee, LF dependence")
{
    const double f50 = binFrequency (50.0, fs, fftN), f1k = binFrequency (1000.0, fs, fftN);
    std::printf ("        IRON hysteresis THD %% @ 50 Hz (rows: amount, cols: -40 -30 -20 -12 -6 0 dBFS)\n");
    const double levels[6] = { -40, -30, -20, -12, -6, 0 };
    double t[4][6];
    const float amounts[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    for (int a = 0; a < 4; ++a)
    {
        std::printf ("        %4.0f%%:", amounts[a] * 100.0);
        for (int l = 0; l < 6; ++l)
        {
            t[a][l] = 100.0 * thdOf (hysteresis (amounts[a], tone (f50, levels[l])), f50);
            std::printf (" %8.4f", t[a][l]);
        }
        std::printf ("\n");
    }

    // Low levels: distortion falls only about in proportion to level
    // (Rayleigh minor loops), unlike a polynomial core (square law).
    const double lowSlope = t[3][1] / t[3][0];     // per 10 dB
    // High levels: the core saturates, THD climbs steeply.
    const double knee = t[3][5] / t[3][3];         // -12 → 0 dBFS
    const double thd1k = 100.0 * thdOf (hysteresis (1.0f, tone (f1k, -12.0)), f1k);
    note ("THD ratio per +10 dB at low level %.2f (square law would be 3.16, cubic 10); -12 → 0 dBFS x%.1f", lowSlope, knee);
    note ("100%%, -12 dBFS: 50 Hz %.3f %%, 1 kHz %.4f %%", t[3][3], thd1k);
    CHECK_RANGE (lowSlope, 2.0, 4.5);
    CHECK (knee > 3.0);
    CHECK (thd1k < 0.05 * t[3][3]);
    for (int a = 1; a < 4; ++a)
        CHECK (t[a][3] > t[a - 1][3]);               // more amount, more iron
    // Same weight as the CLASSIC model at the reference level (-12 dBFS, 50 Hz).
    const double classic[4] = { 0.095, 0.270, 0.521, 0.847 };
    for (int a = 0; a < 4; ++a)
        CHECK_RANGE (t[a][3] / classic[a], 0.6, 1.4);
}

HEAT_TEST ("Hysteresis", "IRON (hysteresis): odd harmonics, no DC, level compensated, exact at 0")
{
    const double f0 = binFrequency (50.0, fs, fftN);
    for (float amount : { 0.5f, 1.0f })
    {
        const auto in = tone (f0, -6.0);
        const auto y = hysteresis (amount, in);
        const double h2 = harmonicDb (y, f0, 2), h3 = harmonicDb (y, f0, 3);
        const double dc = std::abs (meanOf (y.data() + y.size() - fftN, fftN));
        const double level = toDb (rmsOf (y.data() + y.size() - fftN, fftN) / rmsOf (in.data() + in.size() - fftN, fftN));
        note ("amount %.0f%%: H2 %.1f dBc, H3 %.1f dBc, DC %.2e", amount * 100.0, h2, h3, dc);
        note ("level %+.3f dB", level);
        CHECK (h2 < h3 - 30.0);
        CHECK (dc < 1.0e-4);
        CHECK (std::abs (level) < 1.8); // includes the +1.6 dB·amount weight shelf at 50 Hz
    }

    // Programme level (pink-ish noise) changes by a fraction of a dB.
    Noise noise (6);
    std::vector<float> x (96000);
    float lp = 0.0f;
    for (auto& v : x)
    {
        lp += 0.05f * (noise.next() - lp);
        v = 0.5f * lp + 0.05f * noise.next();
    }
    const auto y = hysteresis (1.0f, x);
    const double change = toDb (rmsOf (y.data() + 48000, 48000) / rmsOf (x.data() + 48000, 48000));
    note ("programme level change at 100%%: %+.2f dB", change);
    CHECK (std::abs (change) < 1.0);

    Noise n2;
    std::vector<float> z (4096);
    for (auto& v : z)
        v = n2.next();
    const auto bypassed = hysteresis (0.0f, z, 1);
    double err = 0.0;
    for (size_t i = 0; i < z.size(); ++i)
        err = std::max (err, (double) std::abs (bypassed[i] - z[i]));
    CHECK (err <= 0.0);
}

HEAT_TEST ("Hysteresis", "IRON (hysteresis) remembers its history")
{
    // A loud one-sided low-frequency excursion leaves the core magnetised
    // (remanence). 500 ms later — 25 time constants of the flux integrator's
    // leak, so the flux itself has returned to zero — the same quiet probe
    // runs its minor loops around a different operating point, so it comes
    // out differently after a positive and after a negative history. The
    // difference is compared at the probe's fundamental and harmonics only,
    // which excludes what is left of the DC-blocker tail.
    const double f0 = binFrequency (100.0, fs, fftN);
    auto run = [&] (double burst, IronModel model)
    {
        const int pre = 24000;
        std::vector<float> x (static_cast<size_t> (pre + 16384), 0.0f);
        for (int i = 0; i < 4800; ++i) // 100 ms half-sine: one big one-sided flux swing
            x[static_cast<size_t> (i)] = static_cast<float> (burst * std::sin (pi * i / 4800.0));
        for (int i = pre; i < static_cast<int> (x.size()); ++i)
            x[static_cast<size_t> (i)] = static_cast<float> (0.01 * std::sin (2.0 * pi * f0 * (i - pre) / fs));
        return runStage (StageKind::iron, 1.0f, x, fs, 4, model);
    };
    auto historyEffect = [&] (IronModel model)
    {
        const auto a = run (0.9, model), b = run (-0.9, model);
        std::vector<float> d (a.size());
        for (size_t i = 0; i < d.size(); ++i)
            d[i] = a[i] - b[i];
        const float* w = d.data() + 24000 + 480; // the first probe cycles
        double worst = 0.0;
        for (int h = 1; h <= 3; ++h)
            worst = std::max (worst, toneAmplitude (w, 4800, fs, f0 * h));
        return toDb (worst / 0.01);
    };
    const double hyst = historyEffect (IronModel::hysteresis);
    const double classic = historyEffect (IronModel::classic);
    note ("probe difference after +/- history (dB re probe): hysteresis %.1f dB, classic %.1f dB", hyst, classic);
    CHECK (hyst > -80.0);
    CHECK (hyst > classic + 20.0);
}

HEAT_TEST ("Hysteresis", "IRON (hysteresis): aliasing vs oversampling, extreme inputs")
{
    std::printf ("        IRON hysteresis 100%% @ -3 dBFS, worst non-harmonic product (dBc)\n");
    std::printf ("        freq      2x       4x       8x\n");
    double worst4x = -300.0;
    for (double hz : { 1000.0, 5000.0, 10000.0 })
    {
        const double f0 = binFrequency (hz, fs, fftN);
        std::printf ("        %5.0f", hz);
        for (int factor : { 2, 4, 8 })
        {
            const auto y = hysteresis (1.0f, tone (f0, -3.0), factor);
            const auto r = measureAliasing (y.data() + y.size() - fftN, fftN, fs, f0);
            std::printf ("  %7.1f", r.worstAliasDb);
            if (factor == 4)
                worst4x = std::max (worst4x, r.worstAliasDb);
        }
        std::printf ("\n");
    }
    CHECK (worst4x < -100.0);

    bool finite = true;
    double peak = 0.0;
    for (double hz : { 10.0, 50.0, 1000.0, 20000.0 })
    {
        const auto y = hysteresis (1.0f, tone (hz, 18.0, 48000), 8);
        for (float v : y)
        {
            finite = finite && std::isfinite (v);
            peak = std::max (peak, (double) std::abs (v));
        }
    }
    note ("+18 dBFS 10 Hz – 20 kHz: output peak %.2f", peak);
    CHECK (finite);
    CHECK (peak < 30.0);
}

HEAT_TEST ("Hysteresis", "switching IRON model mid-stream is click-free")
{
    const int n = 96000;
    auto in = stereoSine (fs, 80.0, 0.5, 0.5, n);
    auto p = neutralParams();
    p.iron = 1.0f;
    p.ironModel = IronModel::classic;
    const auto out = runEngine (p, in, fs, 256, nullptr, [] (int start, EngineParams& q)
    {
        q.ironModel = start < 24000 || start >= 72000 ? IronModel::classic : IronModel::hysteresis;
    });
    const double steady = maxSecondDifference (out.l, 12000, 23000);
    double worst = 0.0;
    for (int at : { 24064, 72192 })
        worst = std::max (worst, maxSecondDifference (out.l, at - 256, at + 2400));
    note ("second-difference peak around model changes %.2e vs steady %.2e", worst, steady);
    CHECK (worst < 1.5 * steady);
}
