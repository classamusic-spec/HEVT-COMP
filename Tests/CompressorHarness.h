#pragma once

#include "DSP/CompressorEngine.h"
#include "TestSignals.h"

#include <vector>

namespace heat::test
{
    // Runs the CompressorEngine on a (mono or stereo) sidechain and records
    // the per-sample gain reduction.
    struct CompressorRun
    {
        std::vector<float> grL, grR, makeup, colour;
    };

    inline CompressorRun runCompressor (const dsp::CompressorEngine::Controls& controls,
                                        const std::vector<float>& left,
                                        const std::vector<float>* right = nullptr,
                                        double fs = 48000.0,
                                        int blockSize = 128)
    {
        dsp::CompressorEngine engine;
        engine.prepare (fs);
        engine.snapToControls (controls);
        engine.reset();

        const int n = static_cast<int> (left.size());
        CompressorRun run;
        run.grL.resize (left.size());
        run.grR.resize (left.size());
        run.makeup.resize (left.size());
        run.colour.resize (left.size());

        for (int start = 0; start < n; start += blockSize)
        {
            const int len = std::min (blockSize, n - start);
            engine.setControls (controls, len);
            const float* sc[2] = { left.data() + start, right != nullptr ? right->data() + start : nullptr };
            dsp::CompressorEngine::Outputs out;
            out.grDb[0] = run.grL.data() + start;
            out.grDb[1] = run.grR.data() + start;
            out.makeupDb = run.makeup.data() + start;
            out.colorDrive = run.colour.data() + start;
            engine.process (sc, right != nullptr ? 2 : 1, len, out);
        }
        return run;
    }

    // Steady-state gain reduction for a sine at the given peak level (dBFS).
    // Uses a long release so the measurement reflects the static curve.
    inline double steadyStateGr (dsp::CompressorEngine::Controls controls, double levelDb,
                                 double fs = 48000.0, double hz = 1000.0)
    {
        const int n = static_cast<int> (fs * 1.5);
        const auto x = sine (fs, hz, std::pow (10.0, levelDb / 20.0), n);
        const auto run = runCompressor (controls, x, nullptr, fs);
        const int tail = static_cast<int> (fs * 0.25);
        return meanOf (run.grL.data() + n - tail, tail);
    }
}
