#include "TestFramework.h"
#include "StageHarness.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    constexpr double fs = 48000.0;
    constexpr int fftN = 65536;
}

HEAT_TEST ("Aliasing", "oversampler: passband flat, images rejected")
{
    // Pure up → down round trip (no nonlinearity): passband ripple and
    // image rejection of the half-band cascade.
    for (int factor : { 2, 4, 8 })
    {
        const int stages = factor == 8 ? 3 : factor == 4 ? 2 : 1;
        double worstRipple = 0.0;
        for (double hz : { 100.0, 1000.0, 10000.0, 18000.0, 20000.0 })
        {
            const double f = binFrequency (hz, fs, fftN);
            const auto x = sine (fs, f, 0.5, fftN * 2);
            Oversampler os;
            os.prepare (256);
            os.setNumStages (stages);
            std::vector<float> y (x.size());
            for (size_t s = 0; s < x.size(); s += 256)
            {
                os.upsample (0, x.data() + s, 256);
                os.downsample (0, y.data() + s, 256);
            }
            const double db = toDb (toneAmplitude (y.data() + fftN, fftN, fs, f) / 0.5);
            worstRipple = std::max (worstRipple, std::abs (db));
        }
        note ("x%.0f round trip: worst passband deviation 20 Hz–20 kHz %.4f dB", factor, worstRipple);
        CHECK (worstRipple < 0.05);
    }
}

HEAT_TEST ("Aliasing", "TUBE alias products vs oversampling factor")
{
    std::printf ("        TUBE 100%% @ -3 dBFS, worst non-harmonic product (dBc) in 20 Hz–20 kHz\n");
    std::printf ("        freq      1x       2x       4x       8x\n");
    double table[4][4];
    const double freqs[4] = { 1000.0, 5000.0, 10000.0, 15000.0 };
    const int factors[4] = { 1, 2, 4, 8 };
    for (int fi = 0; fi < 4; ++fi)
    {
        const double f = binFrequency (freqs[fi], fs, fftN);
        const auto x = sine (fs, f, std::pow (10.0, -3.0 / 20.0), fftN * 2);
        std::printf ("        %5.0f", freqs[fi]);
        for (int k = 0; k < 4; ++k)
        {
            const auto y = runStage (StageKind::tube, 1.0f, x, fs, factors[k]);
            table[fi][k] = measureAliasing (y.data() + fftN, fftN, fs, f).worstAliasDb;
            std::printf (" %8.1f", table[fi][k]);
        }
        std::printf ("\n");
    }

    for (int fi = 1; fi < 4; ++fi)
    {
        // Oversampling must buy a large, measurable reduction.
        CHECK (table[fi][2] < table[fi][0] - 15.0);
        CHECK (table[fi][3] <= table[fi][2] + 1.0);
    }
    // HIGH (4x) keeps aliases of a hot 10 kHz tone below -70 dBc.
    CHECK (table[2][2] < -70.0);
}

HEAT_TEST ("Aliasing", "IRON alias products vs oversampling factor")
{
    std::printf ("        IRON 100%% @ -3 dBFS, worst non-harmonic product (dBc)\n");
    std::printf ("        freq      1x       2x       4x       8x\n");
    for (double hz : { 1000.0, 5000.0, 10000.0, 15000.0 })
    {
        const double f = binFrequency (hz, fs, fftN);
        const auto x = sine (fs, f, std::pow (10.0, -3.0 / 20.0), fftN * 2);
        std::printf ("        %5.0f", hz);
        double at4 = 0.0;
        for (int factor : { 1, 2, 4, 8 })
        {
            const auto y = runStage (StageKind::iron, 1.0f, x, fs, factor);
            const double a = measureAliasing (y.data() + fftN, fftN, fs, f).worstAliasDb;
            if (factor == 4)
                at4 = a;
            std::printf (" %8.1f", a);
        }
        std::printf ("\n");
        CHECK (at4 < -90.0);
    }
}
