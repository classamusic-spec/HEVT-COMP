#include "TestFramework.h"
#include "TestSignals.h"
#include "CompressorHarness.h"
#include "DSP/OpticalDetector.h"
#include "DSP/PeakDetector.h"
#include "DSP/RMSDetector.h"

using namespace heat::dsp;
using namespace heat::test;

HEAT_TEST ("Detectors", "PEAK reads instantaneous rectified level")
{
    PeakDetector p;
    for (float x : { 1.0f, -0.5f, 0.25f, -0.001f })
        CHECK_NEAR (p.processDb (x), 20.0 * std::log10 (std::abs (x)), 1.0e-3);
}

HEAT_TEST ("Detectors", "RMS is true power (AES17 sine reference)")
{
    const double fs = 48000.0;
    RMSDetector r;
    r.prepare (fs);

    // Sine: RMS(AES17) must equal the peak level.
    const auto s = sine (fs, 997.0, 0.5, 48000);
    float last = 0.0f;
    double maxDev = 0.0;
    for (int i = 0; i < 48000; ++i)
    {
        last = r.processDb (s[static_cast<size_t> (i)]);
        if (i > 24000)
            maxDev = std::max (maxDev, std::abs (last - toDb (0.5)));
    }
    note ("sine 0.5: RMS(AES17) ripple max deviation %.4f dB", maxDev);
    CHECK (maxDev < 0.1);

    // 10 % duty pulse train. True RMS = A*sqrt(0.1); mean |x| would be 0.1*A.
    r.reset();
    const double A = 0.8;
    // Average the detector's power estimate over whole pulse periods.
    double powerSum = 0.0;
    int powerCount = 0;
    for (int i = 0; i < 96000; ++i)
    {
        const float x = (i % 100) < 10 ? static_cast<float> (A) : 0.0f;
        const float p = r.processPower (x);
        if (i >= 48000)
        {
            powerSum += p;
            ++powerCount;
        }
    }
    const double reading = 10.0 * std::log10 (2.0 * powerSum / powerCount);
    const double expectedTrue = toDb (A * std::sqrt (0.1)) + 3.0103;
    const double expectedAbsMean = toDb (A * 0.1) + 3.0103;
    note ("pulse train: reading %.2f dB, true-RMS %.2f dB, abs-mean would be %.2f dB", reading, expectedTrue, expectedAbsMean);
    CHECK_NEAR (reading, expectedTrue, 0.3);
    CHECK (std::abs (reading - expectedAbsMean) > 5.0);
}

HEAT_TEST ("Detectors", "RMS reads transients lower than PEAK")
{
    const double fs = 48000.0;
    RMSDetector r;
    r.prepare (fs);
    PeakDetector p;

    // 2 ms burst of full-scale noise after silence.
    Noise noise;
    float peakMax = -120.0f, rmsMax = -120.0f;
    for (int i = 0; i < 4800; ++i)
    {
        const float x = (i >= 1000 && i < 1096) ? noise.next() : 0.0f;
        peakMax = std::max (peakMax, p.processDb (x));
        rmsMax = std::max (rmsMax, r.processDb (x));
    }
    note ("2 ms noise burst: PEAK max %.2f dB, RMS max %.2f dB", peakMax, rmsMax);
    CHECK (peakMax - rmsMax > 6.0);
}

HEAT_TEST ("Detectors", "OPTICAL light cell rises faster than it falls")
{
    const double fs = 48000.0;
    OpticalDetector o;
    o.prepare (fs);

    int riseSamples = -1, fallSamples = -1;
    const auto s = sine (fs, 1000.0, 1.0, 48000);
    for (int i = 0; i < 24000; ++i)
    {
        const float db = o.processDb (s[static_cast<size_t> (i)]);
        if (riseSamples < 0 && db > -3.0f)
            riseSamples = i;
    }
    for (int i = 0; i < 24000; ++i)
    {
        const float db = o.processDb (0.0f);
        if (fallSamples < 0 && db < -3.0f)
            fallSamples = i;
    }
    note ("optical cell: -3 dB rise %.2f ms, -3 dB fall %.2f ms", riseSamples / 48.0, fallSamples / 48.0);
    CHECK (riseSamples > 0 && fallSamples > 0);
    CHECK (fallSamples > 3 * riseSamples);
}

HEAT_TEST ("Detectors", "detectors behave measurably differently in the engine")
{
    // Drum-like material: 1.5 ms decaying hits every 250 ms over a quiet bed.
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 3.0);
    std::vector<float> x (static_cast<size_t> (n));
    Noise noise (77);
    for (int i = 0; i < n; ++i)
    {
        const int t = i % 12000;
        const double env = std::exp (-t / (0.0015 * fs));
        x[static_cast<size_t> (i)] = static_cast<float> (0.9 * env * noise.next() + 0.05 * std::sin (2 * pi * 110 * i / fs));
    }

    double maxGr[3] {}, meanGr[3] {};
    std::vector<float> traces[3];
    for (int d = 0; d < 3; ++d)
    {
        CompressorEngine::Controls c;
        c.mode = Mode::clean;
        c.detector = static_cast<Detector> (d);
        c.compress = 0.6f;
        c.attackMs = 0.3f;
        c.releaseMs = 100.0f;
        const auto run = runCompressor (c, x, nullptr, fs);
        const int start = static_cast<int> (fs);
        double mn = 0.0, sum = 0.0;
        for (int i = start; i < n; ++i)
        {
            mn = std::min (mn, (double) run.grL[static_cast<size_t> (i)]);
            sum += run.grL[static_cast<size_t> (i)];
        }
        maxGr[d] = mn;
        meanGr[d] = sum / (n - start);
        traces[d].assign (run.grL.begin() + start, run.grL.end());
    }

    auto traceDistance = [&traces] (int a, int b)
    {
        double s = 0.0;
        for (size_t i = 0; i < traces[a].size(); ++i)
        {
            const double d = traces[a][i] - traces[b][i];
            s += d * d;
        }
        return std::sqrt (s / static_cast<double> (traces[a].size()));
    };
    note ("drum hits: max GR  PEAK %.2f  RMS %.2f  OPTICAL %.2f dB", maxGr[0], maxGr[1], maxGr[2]);
    note ("drum hits: mean GR PEAK %.2f  RMS %.2f  OPTICAL %.2f dB", meanGr[0], meanGr[1], meanGr[2]);

    const double dPR = traceDistance (0, 1), dPO = traceDistance (0, 2), dRO = traceDistance (1, 2);
    note ("GR trace RMS distance: PEAK-RMS %.2f dB, PEAK-OPTICAL %.2f dB, RMS-OPTICAL %.2f dB", dPR, dPO, dRO);

    // PEAK catches the transients hardest.
    CHECK (maxGr[0] < maxGr[1] - 3.0);
    // All three produce clearly different gain-reduction envelopes.
    CHECK (dPR > 1.5);
    CHECK (dPO > 1.5);
    CHECK (dRO > 1.5);
}
