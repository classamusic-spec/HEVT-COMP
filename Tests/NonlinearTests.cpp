#include "TestFramework.h"
#include "EngineHarness.h"
#include "StageHarness.h"

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

    double harmonicDb (const std::vector<float>& y, double f0, int h)
    {
        const int n = fftN;
        const float* x = y.data() + y.size() - static_cast<size_t> (n);
        return toDb (toneAmplitude (x, n, fs, f0 * h) / std::max (1.0e-12, toneAmplitude (x, n, fs, f0)));
    }

    double thdOf (const std::vector<float>& y, double f0)
    {
        const int n = fftN;
        return thd (y.data() + y.size() - static_cast<size_t> (n), n, fs, f0, 12);
    }
}

HEAT_TEST ("Nonlinear", "TUBE and IRON at 0 are exact bypasses")
{
    Noise noise;
    std::vector<float> x (4096);
    for (auto& v : x)
        v = noise.next();
    for (auto kind : { StageKind::tube, StageKind::iron })
    {
        const auto y = runStage (kind, 0.0f, x, fs, 1);
        double err = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            err = std::max (err, (double) std::abs (y[i] - x[i]));
        CHECK (err < 1.0e-7);
    }
}

HEAT_TEST ("Nonlinear", "TUBE: THD vs level and amount (1 kHz, 4x)")
{
    const double f0 = binFrequency (1000.0, fs, fftN);
    std::printf ("        TUBE THD %% (rows: amount, cols: -30 -24 -18 -12 -6 0 dBFS)\n");
    double thdTable[4][6];
    const float amounts[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    const double levels[6] = { -30, -24, -18, -12, -6, 0 };
    for (int a = 0; a < 4; ++a)
    {
        std::printf ("        %4.0f%%:", amounts[a] * 100);
        for (int l = 0; l < 6; ++l)
        {
            const auto y = runStage (StageKind::tube, amounts[a], tone (f0, levels[l]), fs, 4);
            thdTable[a][l] = thdOf (y, f0) * 100.0;
            std::printf (" %7.3f", thdTable[a][l]);
        }
        std::printf ("\n");
    }

    // Calibration targets at -12 dBFS: subtle → clearly warmer → saturated.
    CHECK_RANGE (thdTable[0][3], 0.1, 0.8);
    CHECK_RANGE (thdTable[1][3], 0.6, 3.0);
    CHECK_RANGE (thdTable[3][3], 4.0, 15.0);

    // THD grows with amount at every level, and with level at every amount.
    for (int l = 0; l < 6; ++l)
        for (int a = 1; a < 4; ++a)
            CHECK (thdTable[a][l] > thdTable[a - 1][l]);
    for (int a = 0; a < 4; ++a)
        CHECK (thdTable[a][5] > thdTable[a][0]);
}

HEAT_TEST ("Nonlinear", "TUBE: harmonic balance shifts from H2 to H3 with level")
{
    const double f0 = binFrequency (1000.0, fs, fftN);
    for (double level : { -24.0, -12.0, 0.0 })
    {
        const auto y = runStage (StageKind::tube, 0.6f, tone (f0, level), fs, 4);
        const double h2 = harmonicDb (y, f0, 2), h3 = harmonicDb (y, f0, 3), h4 = harmonicDb (y, f0, 4), h5 = harmonicDb (y, f0, 5);
        std::printf ("      · TUBE 60%% @ %.0f dBFS: H2 %.1f  H3 %.1f  H4 %.1f  H5 %.1f dBc\n", level, h2, h3, h4, h5);
        if (level <= -12.0)
            CHECK (h2 > h3);
    }
    const auto quiet = runStage (StageKind::tube, 0.6f, tone (f0, -24.0), fs, 4);
    const auto loud = runStage (StageKind::tube, 0.6f, tone (f0, 0.0), fs, 4);
    const double balanceQuiet = harmonicDb (quiet, f0, 3) - harmonicDb (quiet, f0, 2);
    const double balanceLoud = harmonicDb (loud, f0, 3) - harmonicDb (loud, f0, 2);
    note ("H3 - H2: %.1f dB at -24 dBFS, %.1f dB at 0 dBFS", balanceQuiet, balanceLoud);
    CHECK (balanceLoud > balanceQuiet + 3.0);
}

HEAT_TEST ("Nonlinear", "TUBE and IRON produce no DC offset")
{
    const double f0 = binFrequency (1000.0, fs, fftN);
    for (auto kind : { StageKind::tube, StageKind::iron })
        for (float amount : { 0.5f, 1.0f })
        {
            const auto y = runStage (kind, amount, tone (kind == StageKind::iron ? 60.0 : f0, -3.0, 480000), fs, 4);
            const double dc = meanOf (y.data() + 240000, 240000);
            note ("stage %.0f (0 = TUBE, 1 = IRON) at %.0f%%: DC %.2e", kind == StageKind::tube ? 0.0 : 1.0, amount * 100, dc);
            CHECK (std::abs (dc) < 1.0e-4);
        }
}

HEAT_TEST ("Nonlinear", "TUBE and IRON are level compensated")
{
    // Broadband programme at -18 dBFS RMS: output level stays within ±1 dB.
    const int n = 96000;
    std::vector<float> x (n);
    Noise noise (9);
    double lp = 0.0;
    for (auto& v : x)
    {
        lp += 0.2 * (noise.next() - lp); // gentle pink-ish tilt
        v = static_cast<float> (lp);
    }
    const double inRms = rmsOf (x.data(), n);
    for (auto& v : x)
        v = static_cast<float> (v * (0.125 / inRms)); // -18 dBFS RMS
    for (auto kind : { StageKind::tube, StageKind::iron })
    {
        std::printf ("        %s level change (dB) at 25/50/75/100%%:", kind == StageKind::tube ? "TUBE" : "IRON");
        for (float amount : { 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto y = runStage (kind, amount, x, fs, 4);
            const double change = toDb (rmsOf (y.data() + n / 2, n / 2) / rmsOf (x.data() + n / 2, n / 2));
            std::printf (" %+.2f", change);
            CHECK (std::abs (change) < 1.0);
        }
        std::printf ("\n");
    }
}

HEAT_TEST ("Nonlinear", "TUBE: small-signal frequency response (HF smoothing)")
{
    for (float amount : { 0.5f, 1.0f })
    {
        std::printf ("        TUBE %3.0f%% @ -40 dBFS:", amount * 100);
        double at10k = 0.0, at15k = 0.0, at100 = 0.0;
        for (double hz : { 30.0, 100.0, 1000.0, 5000.0, 10000.0, 15000.0, 20000.0 })
        {
            const double f = binFrequency (hz, fs, fftN);
            const auto y = runStage (StageKind::tube, amount, tone (f, -40.0), fs, 4);
            const double db = toDb (toneAmplitude (y.data() + y.size() - fftN, fftN, fs, f) / 0.01);
            std::printf (" %.0fHz %+.2f", hz, db);
            if (hz == 100.0) at100 = db;
            if (hz == 10000.0) at10k = db;
            if (hz == 15000.0) at15k = db;
        }
        std::printf ("\n");
        CHECK_NEAR (at100, 0.0, 0.1);
        CHECK (at10k > -1.0);
        if (amount == 1.0f)
            CHECK (at15k < -0.2);
    }
}

HEAT_TEST ("Nonlinear", "IRON: low-frequency dependent saturation and weight")
{
    const double f50 = binFrequency (50.0, fs, fftN), f1k = binFrequency (1000.0, fs, fftN);
    std::printf ("        IRON THD %% at -12 dBFS (cols 25/50/75/100%%):\n");
    double t50[4], t1k[4];
    const float amounts[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    for (int a = 0; a < 4; ++a)
    {
        t50[a] = thdOf (runStage (StageKind::iron, amounts[a], tone (f50, -12.0), fs, 4), f50) * 100.0;
        t1k[a] = thdOf (runStage (StageKind::iron, amounts[a], tone (f1k, -12.0), fs, 4), f1k) * 100.0;
    }
    std::printf ("          50 Hz: %.3f %.3f %.3f %.3f\n", t50[0], t50[1], t50[2], t50[3]);
    std::printf ("           1 kHz: %.4f %.4f %.4f %.4f\n", t1k[0], t1k[1], t1k[2], t1k[3]);
    CHECK_RANGE (t50[3], 0.5, 6.0);
    CHECK (t50[3] > 10.0 * t1k[3]);
    for (int a = 1; a < 4; ++a)
        CHECK (t50[a] > t50[a - 1]);

    // Weight: small-signal low-frequency lift.
    std::printf ("        IRON 100%% small-signal response:");
    double lift40 = 0.0, flat1k = 0.0, at15k = 0.0;
    for (double hz : { 20.0, 40.0, 90.0, 200.0, 1000.0, 10000.0, 15000.0 })
    {
        const double f = binFrequency (hz, fs, fftN);
        const auto y = runStage (StageKind::iron, 1.0f, tone (f, -40.0), fs, 4);
        const double db = toDb (toneAmplitude (y.data() + y.size() - fftN, fftN, fs, f) / 0.01);
        std::printf (" %.0fHz %+.2f", hz, db);
        if (hz == 40.0) lift40 = db;
        if (hz == 1000.0) flat1k = db;
        if (hz == 15000.0) at15k = db;
    }
    std::printf ("\n");
    CHECK_RANGE (lift40, 1.0, 2.0);
    CHECK_NEAR (flat1k, 0.0, 0.15);
    CHECK (at15k > -1.0);
}

HEAT_TEST ("Nonlinear", "extreme inputs stay finite and bounded")
{
    for (auto kind : { StageKind::tube, StageKind::iron })
        for (double hz : { 10.0, 20.0, 1000.0, 20000.0 })
        {
            const auto y = runStage (kind, 1.0f, sine (fs, hz, 8.0, 48000), fs, 8);
            bool finite = true;
            for (auto v : y)
                finite = finite && std::isfinite (v);
            CHECK (finite);
            CHECK (peakOf (y.data(), static_cast<int> (y.size())) < 40.0);
        }
}
