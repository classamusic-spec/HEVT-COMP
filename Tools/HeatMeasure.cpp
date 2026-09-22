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
        for (auto kind : { StageKind::tube, StageKind::iron })
            for (double hz : { 50.0, 1000.0 })
                for (float amount : { 0.25f, 0.5f, 0.75f, 1.0f })
                    for (double level : { -30.0, -24.0, -18.0, -12.0, -6.0, 0.0 })
                    {
                        const double f0 = binFrequency (hz, fs, n);
                        const auto x = sine (fs, f0, std::pow (10.0, level / 20.0), n * 2);
                        const auto y = runStage (kind, amount, x, fs, 4);
                        const float* tail = y.data() + n;
                        const double fund = toneAmplitude (tail, n, fs, f0);
                        f << (kind == StageKind::tube ? "TUBE" : "IRON") << ',' << hz << ',' << amount << ',' << level << ','
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
        for (auto kind : { StageKind::tube, StageKind::iron })
            for (float amount : { 0.5f, 1.0f })
                for (double level : { -12.0, -3.0 })
                    for (double hz : { 1000.0, 5000.0, 10000.0, 15000.0 })
                        for (int factor : { 1, 2, 4, 8 })
                        {
                            const double f0 = binFrequency (hz, fs, n);
                            const auto x = sine (fs, f0, std::pow (10.0, level / 20.0), n * 2);
                            const auto y = runStage (kind, amount, x, fs, factor);
                            const auto r = measureAliasing (y.data() + n, n, fs, f0);
                            f << (kind == StageKind::tube ? "TUBE" : "IRON") << ',' << amount << ',' << level << ','
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

    void cpuCsv (const fsys::path& dir)
    {
        const int seconds = 10;
        const int n = static_cast<int> (fs) * seconds;
        const auto in = programme (n);
        std::ofstream f (dir / "cpu.csv");
        f << "mode,tube,iron,quality,ms_per_second,percent_realtime\n";
        std::printf ("CPU (one stereo instance, 48 kHz, 256-sample blocks):\n");
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
                        StereoBuffer buf = in;
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
                        const char* qn = q == 0 ? "NORMAL" : q == 1 ? "HIGH" : "ULTRA";
                        f << getModeProfile (p.mode).name << ',' << tube << ',' << iron << ',' << qn << ',' << ms << ',' << ms / 10.0 << '\n';
                        std::printf ("  %-5s tube %.1f iron %.1f %-6s  %6.2f ms/s  (%5.2f %% of one core)\n",
                                     getModeProfile (p.mode).name, tube, iron, qn, ms, ms / 10.0);
                    }
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
    cpuCsv (out);
    std::printf ("Measurements written to %s\n", out.string().c_str());
    return 0;
}
