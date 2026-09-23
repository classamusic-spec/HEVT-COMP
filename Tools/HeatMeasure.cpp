// heat_measure — generates the CSV measurements and WAV renders behind the
// quality-gate documents, and profiles CPU. Usage:
//
//   heat_measure <outputDir>
//
// Writes <outputDir>/*.csv and <outputDir>/wav/*.wav

#include "DSP/CompressMacro.h"
#include "DSP/ControlMappings.h"
#include "DSP/GainComputer.h"
#include "DSP/HeatEngine.h"
#include "DSP/Multiband.h"
#include "DSP/SidechainFilter.h"
#include "../Tests/EngineHarness.h"
#include "../Tests/StageHarness.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace heat::dsp;
using namespace heat::test;
namespace fsys = std::filesystem;

namespace
{
    constexpr double fs = 48000.0;

    void writeWav (const fsys::path& path, const StereoBuffer& b, double sampleRate)
    {
        std::ofstream f (path, std::ios::binary);
        const uint32_t n = static_cast<uint32_t> (b.size());
        const uint16_t channels = 2, bits = 32, format = 3; // IEEE float
        const uint32_t byteRate = static_cast<uint32_t> (sampleRate) * channels * bits / 8;
        const uint16_t blockAlign = channels * bits / 8;
        const uint32_t dataBytes = n * blockAlign;
        auto w32 = [&f] (uint32_t v) { f.write (reinterpret_cast<const char*> (&v), 4); };
        auto w16 = [&f] (uint16_t v) { f.write (reinterpret_cast<const char*> (&v), 2); };
        f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVE", 4);
        f.write ("fmt ", 4); w32 (16); w16 (format); w16 (channels);
        w32 (static_cast<uint32_t> (sampleRate)); w32 (byteRate); w16 (blockAlign); w16 (bits);
        f.write ("data", 4); w32 (dataBytes);
        for (uint32_t i = 0; i < n; ++i)
        {
            f.write (reinterpret_cast<const char*> (&b.l[i]), 4);
            f.write (reinterpret_cast<const char*> (&b.r[i]), 4);
        }
    }

    StereoBuffer programme (int n)
    {
        StereoBuffer b (n);
        Noise noise (21);
        double pink = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;
            const int beat = i % static_cast<int> (fs * 0.5);
            const double kick = std::exp (-beat / (0.03 * fs)) * std::sin (2 * pi * (50.0 + 80.0 * std::exp (-beat / (0.01 * fs))) * t);
            const int snarePos = (i + static_cast<int> (fs * 0.25)) % static_cast<int> (fs * 0.5);
            pink += 0.1 * (noise.next() - pink);
            const double snare = std::exp (-snarePos / (0.012 * fs)) * noise.next() * 0.6;
            const double bass = 0.25 * std::sin (2 * pi * 55.0 * t) * (0.6 + 0.4 * std::sin (2 * pi * 0.25 * t));
            const double vocal = 0.18 * (1.0 + 0.8 * std::sin (2 * pi * 0.4 * t)) * std::sin (2 * pi * 220.0 * t + 2.0 * std::sin (2 * pi * 5.0 * t));
            const double v = 0.7 * kick + snare + bass + vocal + 0.03 * pink;
            b.l[static_cast<size_t> (i)] = static_cast<float> (v);
            b.r[static_cast<size_t> (i)] = static_cast<float> (0.92 * v);
        }
        return b;
    }

    void macroCsv (const fsys::path& dir)
    {
        std::ofstream f (dir / "compress_macro.csv");
        f << "mode,compress,threshold_db,ratio,knee_db,makeup_db,gr_at_-24,gr_at_-18,gr_at_-12,gr_at_-6,gr_at_0\n";
        for (int m = 0; m < 3; ++m)
            for (int i = 0; i <= 20; ++i)
            {
                const float c = i / 20.0f;
                const auto r = CompressMacro::evaluate (c, static_cast<Mode> (m));
                f << getModeProfile (static_cast<Mode> (m)).name << ',' << c << ',' << r.thresholdDb << ',' << r.ratio << ','
                  << r.kneeDb << ',' << CompressMacro::makeupDb (r.thresholdDb, r.ratio, r.kneeDb, r.makeupFactor);
                for (float level : { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f })
                    f << ',' << GainComputer::gainReductionDb (level, { r.thresholdDb, r.ratio, r.kneeDb });
                f << '\n';
            }
    }

    void thdCsv (const fsys::path& dir)
    {
        const int n = 65536;
        std::ofstream f (dir / "nonlinear_thd.csv");
        f << "stage,freq_hz,amount,level_dbfs,thd_percent,h2_dbc,h3_dbc,h4_dbc,h5_dbc\n";
        // IRON = CLASSIC model (2.0), IRON_HYST = Jiles-Atherton hysteresis model (2.1 default).
        for (int stage = 0; stage < 3; ++stage)
            for (double hz : { 50.0, 1000.0 })
                for (float amount : { 0.25f, 0.5f, 0.75f, 1.0f })
                    for (double level : { -40.0, -30.0, -24.0, -18.0, -12.0, -6.0, 0.0 })
                    {
                        const auto kind = stage == 0 ? StageKind::tube : StageKind::iron;
                        const auto model = stage == 2 ? IronModel::hysteresis : IronModel::classic;
                        const double f0 = binFrequency (hz, fs, n);
                        const auto x = sine (fs, f0, std::pow (10.0, level / 20.0), n * 2);
                        const auto y = runStage (kind, amount, x, fs, 4, model);
                        const float* tail = y.data() + n;
                        const double fund = toneAmplitude (tail, n, fs, f0);
                        f << (stage == 0 ? "TUBE" : stage == 1 ? "IRON" : "IRON_HYST") << ',' << hz << ',' << amount << ',' << level << ','
                          << thd (tail, n, fs, f0, 12) * 100.0;
                        for (int h = 2; h <= 5; ++h)
                            f << ',' << toDb (toneAmplitude (tail, n, fs, f0 * h) / fund);
                        f << '\n';
                    }
    }

    void aliasCsv (const fsys::path& dir)
    {
        const int n = 65536;
        std::ofstream f (dir / "aliasing.csv");
        f << "stage,amount,level_dbfs,freq_hz,factor,worst_alias_dbc,worst_alias_hz\n";
        for (int stage = 0; stage < 3; ++stage)
            for (float amount : { 0.5f, 1.0f })
                for (double level : { -12.0, -3.0 })
                    for (double hz : { 1000.0, 5000.0, 10000.0, 15000.0 })
                        for (int factor : { 1, 2, 4, 8 })
                        {
                            const auto kind = stage == 0 ? StageKind::tube : StageKind::iron;
                            const auto model = stage == 2 ? IronModel::hysteresis : IronModel::classic;
                            const double f0 = binFrequency (hz, fs, n);
                            const auto x = sine (fs, f0, std::pow (10.0, level / 20.0), n * 2);
                            const auto y = runStage (kind, amount, x, fs, factor, model);
                            const auto r = measureAliasing (y.data() + n, n, fs, f0);
                            f << (stage == 0 ? "TUBE" : stage == 1 ? "IRON" : "IRON_HYST") << ',' << amount << ',' << level << ','
                              << hz << ',' << factor << ',' << r.worstAliasDb << ',' << r.worstAliasHz << '\n';
                        }
    }

    void timingCsv (const fsys::path& dir)
    {
        std::ofstream f (dir / "attack_release.csv");
        f << "panel_position,attack_set_ms,attack_measured_ms,release_set_ms,release_measured_ms\n";
        for (float pos : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const float attack = attackFromNormalised (pos), release = releaseFromNormalised (pos);
            CompressorEngine engine;
            engine.prepare (fs);
            CompressorEngine::Controls c;
            c.mode = Mode::clean;
            c.compress = 0.6f;
            c.attackMs = attack;
            c.releaseMs = release;
            engine.snapToControls (c);
            engine.reset();
            const int n = static_cast<int> (fs * 12.0), up = static_cast<int> (fs), down = static_cast<int> (fs * 5.0);
            std::vector<float> x (static_cast<size_t> (n)), gr (static_cast<size_t> (n));
            for (int i = 0; i < n; ++i)
                x[static_cast<size_t> (i)] = static_cast<float> (((i >= up && i < down) ? 0.5 : 0.0316) * std::sin (2 * pi * 1000.0 * i / fs));
            for (int s = 0; s < n; s += 128)
            {
                const int len = std::min (128, n - s);
                engine.setControls (c, len);
                const float* sc[1] = { x.data() + s };
                CompressorEngine::Outputs o;
                o.grDb[0] = gr.data() + s;
                engine.process (sc, 1, len, o);
            }
            const float hiGr = gr[static_cast<size_t> (down - 1)], loGr = gr[static_cast<size_t> (up - 1)];
            int a = -1, r = -1;
            for (int i = up; i < down && a < 0; ++i)
                if (gr[static_cast<size_t> (i)] <= loGr + 0.632f * (hiGr - loGr)) a = i - up;
            for (int i = down; i < n && r < 0; ++i)
                if (gr[static_cast<size_t> (i)] >= hiGr + 0.632f * (loGr - hiGr)) r = i - down;
            f << pos << ',' << attack << ',' << a * 1000.0 / fs << ',' << release << ',' << r * 1000.0 / fs << '\n';
        }
    }

    // CPU cost of one stereo instance (ms per second of audio): one warm-up
    // pass, then the best of three timed passes (the least disturbed one).
    double runCpu (const EngineParams& p, const StereoBuffer& in, int seconds)
    {
        double best = 1.0e30;
        for (int rep = 0; rep < 4; ++rep)
        {
            StereoBuffer buf = in;
            const int n = buf.size();
            HeatEngine e;
            e.setParams (p);
            e.prepare (fs, 256, 2);
            const auto t0 = std::chrono::steady_clock::now();
            for (int s = 0; s < n; s += 256)
            {
                float* io[2] = { buf.l.data() + s, buf.r.data() + s };
                e.process (io, 2, nullptr, 0, std::min (256, n - s));
            }
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / seconds;
            if (rep > 0)
                best = std::min (best, ms);
        }
        return best;
    }

    void cpuCsv (const fsys::path& dir)
    {
        const int seconds = 10;
        const int n = static_cast<int> (fs) * seconds;
        const auto in = programme (n);
        std::ofstream f (dir / "cpu.csv");
        f << "mode,tube,iron,quality,ms_per_second,percent_realtime\n";
        std::printf ("CPU (one stereo instance, 48 kHz, 256-sample blocks, IRON = HYSTERESIS):\n");
        for (int m = 0; m < 3; ++m)
            for (float tube : { 0.0f, 0.5f })
                for (float iron : { 0.0f, 0.5f })
                    for (int q = 0; q < 3; ++q)
                    {
                        EngineParams p;
                        p.mode = static_cast<Mode> (m);
                        p.tube = tube;
                        p.iron = iron;
                        p.quality = static_cast<Quality> (q);
                        p.compress = 0.6f;
                        const double ms = runCpu (p, in, seconds);
                        const char* qn = q == 0 ? "NORMAL" : q == 1 ? "HIGH" : "ULTRA";
                        f << getModeProfile (p.mode).name << ',' << tube << ',' << iron << ',' << qn << ',' << ms << ',' << ms / 10.0 << '\n';
                        std::printf ("  %-5s tube %.1f iron %.1f %-6s  %6.2f ms/s  (%5.2f %% of one core)\n",
                                     getModeProfile (p.mode).name, tube, iron, qn, ms, ms / 10.0);
                    }
    }


    // --- 2.1 -------------------------------------------------------------------------

    void featureCpuCsv (const fsys::path& dir)
    {
        const int seconds = 10;
        const auto in = programme (static_cast<int> (fs) * seconds);
        std::ofstream f (dir / "cpu_features.csv");
        f << "configuration,ms_per_second,percent_realtime\n";
        std::printf ("CPU per 2.1 feature (WARM, COMPRESS 60 %%, HIGH, one stereo instance, 48 kHz):\n");
        auto base = []
        {
            EngineParams p;
            p.mode = Mode::warm;
            p.compress = 0.6f;
            p.quality = Quality::high;
            return p;
        };
        struct Config { const char* name; EngineParams p; };
        std::vector<Config> configs;
        configs.push_back ({ "baseline (WARM)", base() });
        { auto p = base(); p.iron = 0.5f; p.ironModel = IronModel::classic; configs.push_back ({ "+ IRON 50 % CLASSIC", p }); }
        { auto p = base(); p.iron = 0.5f; p.ironModel = IronModel::hysteresis; configs.push_back ({ "+ IRON 50 % HYSTERESIS", p }); }
        { auto p = base(); p.stereoMode = StereoMode::midSide; configs.push_back ({ "+ M/S", p }); }
        { auto p = base(); p.lookaheadMs = 5.0f; configs.push_back ({ "+ LOOKAHEAD 5 ms", p }); }
        { auto p = base(); p.scLpfHz = 8000.0f; p.scEqDb = 9.0f; configs.push_back ({ "+ SC LPF + BELL", p }); }
        { auto p = base(); p.limiter = true; configs.push_back ({ "+ LIMITER", p }); }
        { auto p = base(); p.multiband = MultibandMode::twoBand; configs.push_back ({ "+ MULTIBAND 2", p }); }
        { auto p = base(); p.multiband = MultibandMode::threeBand; configs.push_back ({ "+ MULTIBAND 3", p }); }
        {
            auto p = base();
            p.tube = 0.5f; p.iron = 0.5f; p.stereoMode = StereoMode::midSide; p.lookaheadMs = 5.0f;
            p.scEqDb = 6.0f; p.limiter = true; p.multiband = MultibandMode::threeBand;
            configs.push_back ({ "everything (+ TUBE 50 %)", p });
            p.quality = Quality::ultra;
            configs.push_back ({ "everything at ULTRA", p });
        }
        for (auto& c : configs)
        {
            const double ms = runCpu (c.p, in, seconds);
            f << '"' << c.name << "\"," << ms << ',' << ms / 10.0 << '\n';
            std::printf ("  %-28s %6.2f ms/s  (%5.2f %% of one core)\n", c.name, ms, ms / 10.0);
        }
    }

    // Independent true-peak meter (16x, 128-tap Kaiser-windowed sinc per phase).
    double truePeak (const std::vector<float>& x, int start, int end)
    {
        const int half = 64, factor = 16;
        auto i0 = [] (double v) { double sum = 1.0, t = 1.0; for (int k = 1; k < 50; ++k) { t *= (v / (2.0 * k)) * (v / (2.0 * k)); sum += t; } return sum; };
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

    void limiterCsv (const fsys::path& dir)
    {
        std::ofstream f (dir / "limiter.csv");
        f << "signal,drive_db,ceiling_dbtp,input_true_peak_dbtp,output_sample_peak_dbfs,output_true_peak_dbtp,over_db\n";
        const int n = static_cast<int> (fs * 4);
        const auto fullBand = programme (n);
        // The same programme band-limited at 20 kHz (8th-order low-pass).
        StereoBuffer limited = fullBand;
        for (auto* ch : { &limited.l, &limited.r })
        {
            TptSvf s[4];
            for (auto& x : s)
                x.design (20000.0, fs, 1.0 / 0.7071);
            for (auto& v : *ch)
                for (auto& x : s)
                    v = x.lowpass (v);
        }
        for (int band = 0; band < 2; ++band)
            for (float ceiling : { -1.0f, -0.3f })
                for (float drive : { 0.0f, 6.0f, 12.0f })
                {
                    const auto& prog = band == 0 ? limited : fullBand;
                    auto p = neutralParams();
                    p.limiter = true;
                    p.ceilingDb = ceiling;
                    p.inputDb = drive;
                    const auto out = runEngine (p, prog, fs);
                    StereoBuffer scaled = prog;
                    for (auto& v : scaled.l)
                        v *= dbToGain (drive);
                    const double inTp = toDb (truePeak (scaled.l, 4800, n));
                    const double sp = toDb (peakOf (out.l.data() + 4800, n - 4800));
                    const double tp = toDb (truePeak (out.l, 4800, n));
                    f << (band == 0 ? "programme (20 kHz band-limited)," : "programme (full band 24 kHz, white-noise snare),")
                      << drive << ',' << ceiling << ',' << inTp << ',' << sp << ',' << tp << ',' << tp - ceiling << '\n';
                }
        for (double hz : { 997.0, 11025.0, 12000.0 })
        {
            auto p = neutralParams();
            p.limiter = true;
            p.ceilingDb = -1.0f;
            StereoBuffer b (n);
            for (int i = 0; i < n; ++i)
                b.l[static_cast<size_t> (i)] = b.r[static_cast<size_t> (i)] = static_cast<float> (1.2 * std::sin (2.0 * pi * hz * i / fs + 0.25 * pi));
            const auto out = runEngine (p, b, fs);
            const double inTp = toDb (truePeak (b.l, 4800, n));
            const double tp = toDb (truePeak (out.l, 4800, n));
            f << "sine " << hz << " Hz,0,-1," << inTp << ',' << toDb (peakOf (out.l.data() + 4800, n - 4800)) << ',' << tp << ',' << tp + 1.0 << '\n';
        }
    }

    void lookaheadCsv (const fsys::path& dir)
    {
        std::ofstream f (dir / "lookahead.csv");
        f << "lookahead_ms,attack_ms,onset_overshoot_db,predicted_db,latency_samples\n";
        const int n = 48000, step = 24000;
        StereoBuffer in (n);
        for (int i = 0; i < n; ++i)
            in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)] = (i < step ? 0.01f : 0.9f) * static_cast<float> (std::sin (2.0 * pi * 1000.0 * i / fs));
        for (float attack : { 2.0f, 5.0f })
        {
            double without = 0.0;
            for (float la : { 0.0f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f })
            {
                auto p = neutralParams();
                p.compress = 0.8f;
                p.attackMs = attack;
                p.releaseMs = 200.0f;
                p.autoMakeup = false;
                p.lookaheadMs = la;
                HeatEngine e;
                const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
                const int lat = e.getLatencySamples();
                const double onset = peakOf (out.l.data() + step + lat, static_cast<int> (0.002 * fs));
                const double settled = peakOf (out.l.data() + step + lat + 4800, 480);
                const double over = toDb (onset / settled);
                if (la <= 0.0f)
                    without = over;
                f << la << ',' << attack << ',' << over << ',' << without * std::exp (-la / attack) << ',' << lat << '\n';
            }
        }
    }

    void multibandCsv (const fsys::path& dir)
    {
        std::ofstream f (dir / "multiband.csv");
        f << "multiband,presence_swing_db,low_band_gr_db,mid_band_gr_db,high_band_gr_db,neutral_null\n";
        const int n = 96000;
        StereoBuffer in (n);
        for (int i = 0; i < n; ++i)
        {
            const double t = static_cast<double> (i) / fs;
            const double env = 0.5 + 0.5 * std::sin (2.0 * pi * 2.0 * t);
            in.l[static_cast<size_t> (i)] = in.r[static_cast<size_t> (i)]
                = static_cast<float> (0.8 * env * std::sin (2.0 * pi * 60.0 * t) + 0.05 * std::sin (2.0 * pi * 3000.0 * t));
        }
        for (int mb = 0; mb < 3; ++mb)
        {
            auto p = neutralParams();
            p.compress = 0.75f;
            p.attackMs = 5.0f;
            p.releaseMs = 60.0f;
            p.multiband = static_cast<MultibandMode> (mb);
            p.xoverLowHz = 200.0f;
            p.xoverHighHz = 2000.0f;
            HeatEngine e;
            const auto out = runEngine (p, in, fs, 256, nullptr, {}, &e);
            TptSvf bp;
            bp.design (3000.0, fs, 0.05);
            double lo = 1.0e9, hi = 0.0, env = 0.0;
            for (int i = 0; i < n; ++i)
            {
                float v1, v2;
                bp.tick (out.l[static_cast<size_t> (i)], v1, v2);
                env = std::max (std::abs ((double) v1) * 0.05, env * 0.999);
                if (i > n / 2) { lo = std::min (lo, env); hi = std::max (hi, env); }
            }
            // Neutral null for the same mode.
            auto pn = neutralParams();
            pn.multiband = p.multiband;
            HeatEngine en;
            const auto nOut = runEngine (pn, in, fs, 256, nullptr, {}, &en);
            const int lat = en.getLatencySamples();
            double err = 0.0;
            for (int i = lat; i < n; ++i)
                err = std::max (err, (double) std::abs (nOut.l[static_cast<size_t> (i)] - in.l[static_cast<size_t> (i - lat)]));
            const auto& t = e.getTelemetry();
            f << (mb == 0 ? "OFF" : mb == 1 ? "2 BAND" : "3 BAND") << ',' << toDb (hi / lo) << ',' << t.bandGrDb[0] << ','
              << t.bandGrDb[1] << ',' << t.bandGrDb[2] << ',' << err << '\n';
        }
    }

    void sidechainEqCsv (const fsys::path& dir)
    {
        std::ofstream f (dir / "sidechain_eq.csv");
        f << "freq_hz,default_db,hpf120_lpf6k_db,bell_7k_plus15_q1.5_db,bell_300_minus9_q0.7_db\n";
        for (double hz = 20.0; hz <= 20000.0; hz *= std::pow (2.0, 1.0 / 6.0))
            f << hz << ','
              << toDb (SidechainFilter::chainMagnitudeAt (hz, 20.0, 20000.0, 3000.0, 0.0, 1.0, fs)) << ','
              << toDb (SidechainFilter::chainMagnitudeAt (hz, 120.0, 6000.0, 3000.0, 0.0, 1.0, fs)) << ','
              << toDb (SidechainFilter::chainMagnitudeAt (hz, 20.0, 20000.0, 7000.0, 15.0, 1.5, fs)) << ','
              << toDb (SidechainFilter::chainMagnitudeAt (hz, 20.0, 20000.0, 300.0, -9.0, 0.7, fs)) << '\n';
    }

    void renderWavs (const fsys::path& dir)
    {
        const int n = static_cast<int> (fs * 8);
        const auto in = programme (n);
        writeWav (dir / "00_input_programme.wav", in, fs);

        struct Render { const char* name; EngineParams p; };
        std::vector<Render> renders;
        auto add = [&renders] (const char* name, Mode m, Detector d, float c, float tube, float iron, float mix)
        {
            EngineParams p;
            p.mode = m;
            p.detector = d;
            p.compress = c;
            p.tube = tube;
            p.iron = iron;
            p.mix = mix;
            renders.push_back ({ name, p });
        };
        add ("01_clean_peak_50.wav", Mode::clean, Detector::peak, 0.5f, 0, 0, 1);
        add ("02_clean_rms_50.wav", Mode::clean, Detector::rms, 0.5f, 0, 0, 1);
        add ("03_clean_optical_50.wav", Mode::clean, Detector::optical, 0.5f, 0, 0, 1);
        add ("04_warm_peak_50.wav", Mode::warm, Detector::peak, 0.5f, 0, 0, 1);
        add ("05_drive_peak_75.wav", Mode::drive, Detector::peak, 0.75f, 0, 0, 1);
        add ("06_warm_tube50_iron50.wav", Mode::warm, Detector::peak, 0.5f, 0.5f, 0.5f, 1);
        add ("07_drive_tube100_iron100.wav", Mode::drive, Detector::peak, 1.0f, 1.0f, 1.0f, 1);
        add ("08_parallel_drive_mix40.wav", Mode::drive, Detector::peak, 1.0f, 0.6f, 0.3f, 0.4f);
        {
            EngineParams p; p.mode = Mode::warm; p.compress = 0.5f; p.stereoMode = StereoMode::midSide; p.link = StereoLink::dualMono;
            renders.push_back ({ "09_warm_ms_dualmono.wav", p });
        }
        {
            EngineParams p; p.mode = Mode::clean; p.detector = Detector::rms; p.compress = 0.6f; p.multiband = MultibandMode::threeBand;
            renders.push_back ({ "10_clean_multiband3.wav", p });
        }
        {
            EngineParams p; p.mode = Mode::clean; p.compress = 0.3f; p.inputDb = 8.0f; p.limiter = true; p.ceilingDb = -1.0f;
            renders.push_back ({ "11_limiter_-1dBTP_drive8.wav", p });
        }
        {
            EngineParams p; p.mode = Mode::warm; p.compress = 0.4f; p.iron = 1.0f; p.ironModel = IronModel::hysteresis;
            renders.push_back ({ "12_iron100_hysteresis.wav", p });
            p.ironModel = IronModel::classic;
            renders.push_back ({ "13_iron100_classic.wav", p });
        }
        {
            EngineParams p; p.mode = Mode::drive; p.compress = 0.7f; p.attackMs = 0.5f; p.lookaheadMs = 2.0f;
            renders.push_back ({ "14_drive_lookahead2ms.wav", p });
        }
        for (auto& r : renders)
            writeWav (dir / r.name, runEngine (r.p, in, fs), fs);
    }
}

int main (int argc, char** argv)
{
    const fsys::path out = argc > 1 ? argv[1] : "measurements";
    fsys::create_directories (out / "wav");

    macroCsv (out);
    timingCsv (out);
    thdCsv (out);
    aliasCsv (out);
    renderWavs (out / "wav");
    limiterCsv (out);
    lookaheadCsv (out);
    multibandCsv (out);
    sidechainEqCsv (out);
    cpuCsv (out);
    featureCpuCsv (out);
    std::printf ("Measurements written to %s\n", out.string().c_str());
    return 0;
}
