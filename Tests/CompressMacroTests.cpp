#include "TestFramework.h"
#include "CompressorHarness.h"
#include "DSP/CompressMacro.h"
#include "DSP/GainComputer.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    const char* modeName (int m) { return m == 0 ? "CLEAN" : m == 1 ? "WARM" : "DRIVE"; }

    float staticGr (Mode mode, float compress, float levelDb)
    {
        const auto r = CompressMacro::evaluate (compress, mode);
        return GainComputer::gainReductionDb (levelDb, { r.thresholdDb, r.ratio, r.kneeDb });
    }
}

HEAT_TEST ("CompressMacro", "COMPRESS 0% leaves nominal signals untouched")
{
    for (int m = 0; m < 3; ++m)
        for (float level : { -40.0f, -18.0f, -12.0f, -6.0f, 0.0f })
            CHECK_NEAR (staticGr (static_cast<Mode> (m), 0.0f, level), 0.0, 1.0e-4);
}

HEAT_TEST ("CompressMacro", "GR increases monotonically with COMPRESS, no jumps")
{
    for (int m = 0; m < 3; ++m)
    {
        const auto mode = static_cast<Mode> (m);
        for (float level : { -30.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f })
        {
            float prev = 0.0f;
            bool monotonic = true;
            float maxStep = 0.0f;
            for (int i = 0; i <= 1000; ++i)
            {
                const float c = i / 1000.0f;
                const float gr = staticGr (mode, c, level);
                monotonic = monotonic && gr <= prev + 1.0e-5f;
                maxStep = std::max (maxStep, prev - gr);
                prev = gr;
            }
            CHECK (monotonic);
            // 0.1 % of travel may never move GR by more than 0.25 dB.
            CHECK (maxStep < 0.25f);
        }

        // Hidden expert parameters move in the expected direction.
        float prevThr = 1.0e9f, prevRatio = 0.0f;
        bool thrOk = true, ratioOk = true;
        for (int i = 0; i <= 100; ++i)
        {
            const auto r = CompressMacro::evaluate (i / 100.0f, mode);
            thrOk = thrOk && r.thresholdDb <= prevThr + 1.0e-5f;
            ratioOk = ratioOk && r.ratio >= prevRatio - 1.0e-5f;
            prevThr = r.thresholdDb;
            prevRatio = r.ratio;
        }
        CHECK (thrOk);
        CHECK (ratioOk);
    }
}

HEAT_TEST ("CompressMacro", "calibration: gentle / clear / strong / extreme")
{
    // Static GR (dB) for a -12 dBFS sine at 0/25/50/75/100 %.
    struct Target { float lo, hi; };
    const Target targets[3][5] = {
        /* CLEAN */ { { 0, 0.05f }, { 1.0f, 3.0f }, { 4.0f, 7.5f }, { 9.0f, 14.0f }, { 17.0f, 26.0f } },
        /* WARM  */ { { 0, 0.05f }, { 1.0f, 3.0f }, { 3.5f, 7.0f }, { 7.5f, 12.0f }, { 13.0f, 21.0f } },
        /* DRIVE */ { { 0, 0.05f }, { 1.5f, 4.0f }, { 5.0f, 9.0f }, { 11.0f, 17.0f }, { 20.0f, 30.0f } },
    };

    for (int m = 0; m < 3; ++m)
    {
        double gr[5];
        for (int k = 0; k < 5; ++k)
        {
            gr[k] = -staticGr (static_cast<Mode> (m), k * 0.25f, -12.0f);
            CHECK_RANGE (gr[k], targets[m][k].lo, targets[m][k].hi);
        }
        std::printf ("        %s  -12 dBFS static GR: 0%% %.2f | 25%% %.2f | 50%% %.2f | 75%% %.2f | 100%% %.2f dB\n",
                     modeName (m), gr[0], gr[1], gr[2], gr[3], gr[4]);
    }
}

HEAT_TEST ("CompressMacro", "engine sweep: measured GR and output level vs COMPRESS")
{
    // Full engine (detector + ballistics + makeup) with a steady -12 dBFS sine.
    for (int m = 0; m < 3; ++m)
    {
        double prevGr = 0.0, prevOut = -12.0;
        bool monotonic = true;
        double maxOutStep = 0.0;
        std::printf ("        %s sweep (c, GR dB, makeup dB, out dBFS):", modeName (m));
        for (int i = 0; i <= 20; ++i)
        {
            CompressorEngine::Controls c;
            c.mode = static_cast<Mode> (m);
            c.detector = Detector::peak;
            c.compress = i / 20.0f;
            c.attackMs = 3.0f;
            c.releaseMs = 1000.0f;
            const int n = 48000;
            const auto x = sine (48000.0, 1000.0, std::pow (10.0, -12.0 / 20.0), n);
            const auto run = runCompressor (c, x);
            const int tail = 12000;
            const double gr = meanOf (run.grL.data() + n - tail, tail);
            const double mk = meanOf (run.makeup.data() + n - tail, tail);
            const double out = -12.0 + gr + mk;
            monotonic = monotonic && gr <= prevGr + 0.02;
            maxOutStep = std::max (maxOutStep, std::abs (out - prevOut));
            prevGr = gr;
            prevOut = out;
            if (i % 4 == 0)
                std::printf (" (%.2f, %.2f, %.2f, %.2f)", c.compress, gr, mk, out);
        }
        std::printf ("\n");
        CHECK (monotonic);
        // No sudden loudness spikes: a 5 % step never changes output by > 1.5 dB.
        CHECK (maxOutStep < 1.5);
    }
}

HEAT_TEST ("CompressMacro", "auto makeup is conservative")
{
    for (int m = 0; m < 3; ++m)
        for (int i = 0; i <= 10; ++i)
        {
            const auto r = CompressMacro::evaluate (i / 10.0f, static_cast<Mode> (m));
            const float mk = CompressMacro::makeupDb (r.thresholdDb, r.ratio, r.kneeDb, r.makeupFactor);
            const float grRef = -staticGr (static_cast<Mode> (m), i / 10.0f, CompressMacro::makeupReferenceDb);
            CHECK (mk >= 0.0f);
            CHECK (mk <= grRef + 1.0e-4f);           // never more than the reduction at the reference level
            CHECK (mk <= CompressMacro::makeupLimitDb);
        }
}
