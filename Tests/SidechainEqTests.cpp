#include "TestFramework.h"
#include "EngineHarness.h"
#include "DSP/SidechainFilter.h"

using namespace heat::dsp;
using namespace heat::test;

namespace
{
    double measureChain (double hz, float hpf, float lpf, float bellHz, float bellDb, float bellQ, double fs)
    {
        SidechainFilter f;
        f.prepare (fs);
        f.setCutoffImmediate (hpf);
        f.setLowpassImmediate (lpf);
        f.setBellImmediate (bellHz, bellDb, bellQ);
        const int n = static_cast<int> (fs);
        auto x = sine (fs, hz, 0.25, n);
        float* ch[1] = { x.data() };
        f.process (ch, 1, n);
        return toDb (toneAmplitude (x.data() + n / 2, n / 2, fs, hz) / 0.25);
    }
}

HEAT_TEST ("SidechainEQ", "LPF: -3 dB at cutoff, 12 dB/oct above, bypassed at 20 kHz")
{
    const double fs = 48000.0;
    for (float cutoff : { 1000.0f, 4000.0f, 12000.0f })
    {
        const double atFc = measureChain (cutoff, 20.0f, cutoff, 3000.0f, 0.0f, 1.0f, fs);
        const double octaveAbove = cutoff * 2.0 < 0.45 * fs ? measureChain (cutoff * 2.0, 20.0f, cutoff, 3000.0f, 0.0f, 1.0f, fs) : atFc - 9.0;
        const double decadeBelow = measureChain (cutoff / 10.0, 20.0f, cutoff, 3000.0f, 0.0f, 1.0f, fs);
        note ("LPF %.0f Hz: @fc %.2f dB, @2fc %.2f dB, @fc/10 %.3f dB", cutoff, atFc, octaveAbove, decadeBelow);
        CHECK_NEAR (atFc, -3.01, 0.15);
        CHECK (octaveAbove - atFc < -8.0);
        CHECK_NEAR (decadeBelow, 0.0, 0.05);
        CHECK_NEAR (atFc, toDb (SidechainFilter::lowpassMagnitudeAt (cutoff, cutoff, fs)), 0.1);
    }

    // At the top of the range the LPF is out of the circuit: exact identity
    // (besides the HPF, which is itself at 20 Hz).
    SidechainFilter a, b;
    for (auto* f : { &a, &b })
    {
        f->prepare (fs);
        f->setCutoffImmediate (20.0f);
    }
    a.setLowpassImmediate (20000.0f);
    Noise noise (3);
    std::vector<float> x (4096), y;
    for (auto& v : x)
        v = noise.next();
    y = x;
    float* xa[1] = { x.data() };
    a.process (xa, 1, 4096);
    SidechainFilter hpfOnly;
    hpfOnly.prepare (fs);
    hpfOnly.setCutoffImmediate (20.0f);
    float* ya[1] = { y.data() };
    hpfOnly.process (ya, 1, 4096);
    double diff = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
        diff = std::max (diff, (double) std::abs (x[i] - y[i]));
    note ("LPF at 20 kHz vs no LPF: max difference %.2e", diff);
    CHECK (diff <= 0.0);
}

HEAT_TEST ("SidechainEQ", "BELL: gain at the centre, bandwidth follows Q, 0 dB is exact")
{
    const double fs = 48000.0;
    for (float gainDb : { 12.0f, -9.0f, 18.0f })
        for (float q : { 0.5f, 1.0f, 4.0f })
        {
            const float fc = 3000.0f;
            const double centre = measureChain (fc, 20.0f, 20000.0f, fc, gainDb, q, fs);
            const double far = measureChain (100.0, 20.0f, 20000.0f, fc, gainDb, q, fs);
            const double model = toDb (SidechainFilter::bellMagnitudeAt (fc, fc, gainDb, q, fs));
            CHECK_NEAR (centre, gainDb, 0.15);
            CHECK_NEAR (centre, model, 0.1);
            CHECK (std::abs (far) < (q >= 1.0f ? 0.3 : 1.2));
        }
    // Wider Q → more gain an octave away.
    const double narrowOct = measureChain (6000.0, 20.0f, 20000.0f, 3000.0f, 12.0f, 4.0f, fs);
    const double wideOct = measureChain (6000.0, 20.0f, 20000.0f, 3000.0f, 12.0f, 0.5f, fs);
    note ("+12 dB bell at 3 kHz, one octave above: Q 4 %.2f dB, Q 0.5 %.2f dB", narrowOct, wideOct);
    CHECK (wideOct > narrowOct + 4.0);

    const double off = measureChain (3000.0, 20.0f, 20000.0f, 3000.0f, 0.0f, 1.0f, fs);
    CHECK_NEAR (off, 0.0, 0.01);
}

HEAT_TEST ("SidechainEQ", "a sidechain bell turns the compressor into a de-esser")
{
    // Equal-level 1 kHz and 7 kHz tones. With +15 dB at 7 kHz in the detector,
    // the sibilant tone is compressed much harder than the vocal tone.
    const double fs = 48000.0;
    const int n = 48000;
    auto grFor = [&] (double hz, float bellDb)
    {
        auto p = neutralParams();
        p.compress = 0.5f;
        p.attackMs = 1.0f;
        p.releaseMs = 80.0f;
        p.scEqHz = 7000.0f;
        p.scEqDb = bellDb;
        p.scEqQ = 1.5f;
        HeatEngine e;
        runEngine (p, stereoSine (fs, hz, 0.12, 0.12, n), fs, 256, nullptr, {}, &e);
        return (double) e.getTelemetry().grDbLast[0];
    };
    const double flat1k = grFor (1000.0, 0.0f), flat7k = grFor (7000.0, 0.0f);
    const double bell1k = grFor (1000.0, 15.0f), bell7k = grFor (7000.0, 15.0f);
    note ("GR without bell: 1 kHz %.2f dB, 7 kHz %.2f dB", flat1k, flat7k);
    note ("GR with +15 dB @ 7 kHz: 1 kHz %.2f dB, 7 kHz %.2f dB", bell1k, bell7k);
    CHECK_NEAR (flat1k, flat7k, 0.3);
    CHECK (bell7k < flat7k - 4.0);
    CHECK_NEAR (bell1k, flat1k, 0.5);
}

HEAT_TEST ("SidechainEQ", "the audible path is untouched and modulation is stable")
{
    const double fs = 48000.0;
    const int n = 48000;
    Noise noise (17);
    StereoBuffer in (n);
    for (int i = 0; i < n; ++i)
        in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] = 0.3f * noise.next();

    // COMPRESS 0: the sidechain EQ must not reach the audio at all.
    auto p = neutralParams();
    p.scLpfHz = 2000.0f;
    p.scEqDb = 18.0f;
    p.scEqHz = 500.0f;
    HeatEngine e;
    const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
    const int lat = e.getLatencySamples();
    double err = 0.0;
    for (int i = lat; i < n; ++i)
        err = std::max (err, (double) std::abs (out.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
    CHECK (err < 1.0e-6);

    // Fast sweeps of every EQ control while compressing: finite and bounded.
    auto pc = neutralParams();
    pc.compress = 0.8f;
    const auto swept = runEngine (pc, in, fs, 64, nullptr, [] (int start, EngineParams& q)
    {
        const double ph = start / 1200.0;
        q.scLpfHz = static_cast<float> (1000.0 * std::pow (20.0, 0.5 + 0.5 * std::sin (ph)));
        q.scEqHz = static_cast<float> (100.0 * std::pow (100.0, 0.5 + 0.5 * std::sin (1.7 * ph)));
        q.scEqDb = static_cast<float> (18.0 * std::sin (2.3 * ph));
        q.scEqQ = static_cast<float> (std::pow (10.0, std::sin (0.9 * ph)));
    });
    bool finite = true;
    double peak = 0.0;
    for (float v : swept.l)
    {
        finite = finite && std::isfinite (v);
        peak = std::max (peak, (double) std::abs (v));
    }
    note ("neutral null %.2e; swept EQ output peak %.3f", err, peak);
    CHECK (finite);
    CHECK (peak < 1.0);
}
