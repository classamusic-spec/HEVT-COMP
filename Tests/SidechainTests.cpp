#include "TestFramework.h"
#include "CompressorHarness.h"
#include "EngineHarness.h"
#include "DSP/SidechainFilter.h"

using namespace heat::dsp;
using namespace heat::test;

HEAT_TEST ("Sidechain", "HPF response: -3 dB at cutoff, 12 dB/oct below")
{
    const double fs = 48000.0;
    for (float cutoff : { 20.0f, 90.0f, 400.0f })
    {
        SidechainFilter f;
        f.prepare (fs);
        f.setCutoffImmediate (cutoff);

        auto measure = [&] (double hz)
        {
            f.reset();
            const int n = static_cast<int> (fs * 2);
            auto x = sine (fs, hz, 0.5, n);
            float* ch[1] = { x.data() };
            f.process (ch, 1, n);
            return toDb (toneAmplitude (x.data() + n / 2, n / 2, fs, hz) / 0.5);
        };

        const double atCutoff = measure (cutoff);
        const double octaveBelow = measure (cutoff / 2.0);
        const double decadeAbove = measure (cutoff * 10.0);
        note ("HPF %.0f Hz: @fc %.2f dB, @fc/2 %.2f dB, @10fc %.3f dB", cutoff, atCutoff, octaveBelow, decadeAbove);
        CHECK_NEAR (atCutoff, -3.01, 0.15);
        CHECK_NEAR (octaveBelow - atCutoff, -9.3, 1.0); // 2nd-order Butterworth: |H(fc/2)| = -12.3 dB
        CHECK_NEAR (decadeAbove, 0.0, 0.05);
        CHECK_NEAR (atCutoff, toDb (SidechainFilter::magnitudeAt (cutoff, cutoff, fs)), 0.1);
    }
}

HEAT_TEST ("Sidechain", "HPF reduces bass-driven gain reduction")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs * 1.5);
    std::vector<float> x (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        x[static_cast<size_t> (i)] = static_cast<float> (0.6 * std::sin (2 * pi * 50 * i / fs) + 0.1 * std::sin (2 * pi * 1500 * i / fs));

    double gr[2];
    for (int k = 0; k < 2; ++k)
    {
        auto p = neutralParams();
        p.compress = 0.6f;
        p.hpfHz = k == 0 ? 20.0f : 400.0f;
        StereoBuffer in (n);
        in.l = x;
        in.r = x;
        HeatEngine e;
        runEngine (p, in, fs, 256, nullptr, {}, &e);
        gr[k] = e.getTelemetry().grDbLast[0];
    }
    note ("50 Hz-heavy signal: GR with HPF 20 Hz %.2f dB, with HPF 400 Hz %.2f dB", gr[0], gr[1]);
    CHECK (gr[1] > gr[0] + 4.0);
}

HEAT_TEST ("Sidechain", "HPF never filters the audible signal")
{
    // With COMPRESS at 0 the output must be the delayed input regardless of HPF.
    const double fs = 48000.0;
    const int n = 24000;
    for (float hpf : { 20.0f, 150.0f, 400.0f })
    {
        auto p = neutralParams();
        p.hpfHz = hpf;
        auto in = stereoSine (fs, 40.0, 0.5, 0.5, n);
        const auto out = runEngine (p, in, fs);
        HeatEngine e;
        e.setParams (p);
        e.prepare (fs, 256, 2);
        const int lat = e.getLatencySamples();
        double err = 0.0;
        for (int i = lat; i < n; ++i)
            err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
        note ("HPF %.0f Hz: max deviation from delayed input %.2e", hpf, err);
        CHECK (err < 1.0e-6);
    }
}

HEAT_TEST ("Sidechain", "HPF stays stable under fast cutoff modulation")
{
    const double fs = 48000.0;
    SidechainFilter f;
    f.prepare (fs);
    Noise noise;
    std::vector<float> x (256);
    float peak = 0.0f;
    for (int block = 0; block < 2000; ++block)
    {
        f.setCutoff (block % 2 == 0 ? 20.0f : 400.0f);
        for (auto& v : x)
            v = noise.next();
        float* ch[1] = { x.data() };
        f.process (ch, 1, 256);
        for (auto v : x)
            peak = std::max (peak, std::abs (v));
    }
    note ("modulated HPF peak output %.3f", peak);
    CHECK (std::isfinite (peak));
    CHECK (peak < 4.0f);
}

HEAT_TEST ("Sidechain", "EXTERNAL key drives compression of the main signal")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs);
    auto main = stereoSine (fs, 1000.0, 0.05, 0.05, n);  // quiet programme
    auto key = stereoSine (fs, 200.0, 0.8, 0.8, n);      // loud key

    auto p = neutralParams();
    p.compress = 0.6f;
    p.scSource = SidechainSource::external;
    HeatEngine eExt;
    runEngine (p, main, fs, 256, &key, {}, &eExt);

    p.scSource = SidechainSource::internal;
    HeatEngine eInt;
    runEngine (p, main, fs, 256, &key, {}, &eInt);

    // External selected but no key connected → falls back to internal.
    p.scSource = SidechainSource::external;
    HeatEngine eMissing;
    runEngine (p, main, fs, 256, nullptr, {}, &eMissing);

    note ("GR: external key %.2f dB, internal %.2f dB, external w/o key %.2f dB",
          eExt.getTelemetry().grDbLast[0], eInt.getTelemetry().grDbLast[0], eMissing.getTelemetry().grDbLast[0]);
    CHECK (eExt.getTelemetry().grDbLast[0] < -6.0f);
    CHECK (eInt.getTelemetry().grDbLast[0] > -0.5f);
    CHECK_NEAR (eMissing.getTelemetry().grDbLast[0], eInt.getTelemetry().grDbLast[0], 0.01);
}

HEAT_TEST ("Sidechain", "SC LISTEN outputs the filtered detector signal")
{
    const double fs = 48000.0;
    const int n = static_cast<int> (fs);
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
        in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] =
            static_cast<float> (0.5 * std::sin (2 * pi * 30 * i / fs) + 0.2 * std::sin (2 * pi * 3000 * i / fs));

    auto p = neutralParams();
    p.scListen = true;
    p.hpfHz = 400.0f;
    const auto out = runEngine (p, in, fs);
    const int m = n / 2;
    const double low = toneAmplitude (out.l.data() + m, m, fs, 30.0);
    const double high = toneAmplitude (out.l.data() + m, m, fs, 3000.0);
    note ("SC listen with HPF 400: 30 Hz %.2f dB, 3 kHz %.2f dB", toDb (low / 0.5), toDb (high / 0.2));
    CHECK (toDb (low / 0.5) < -40.0);
    CHECK_NEAR (toDb (high / 0.2), 0.0, 0.2);
}
