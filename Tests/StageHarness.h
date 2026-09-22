#pragma once

#include "Nonlinear/IronStage.h"
#include "Nonlinear/ModeColor.h"
#include "Nonlinear/TubeStage.h"
#include "Quality/Oversampler.h"
#include "TestSignals.h"

#include <vector>

namespace heat::test
{
    enum class StageKind { tube, iron };

    // Runs one nonlinear stage in isolation at a given oversampling factor
    // (1 = no oversampling) exactly as the engine does: up → stage → down.
    inline std::vector<float> runStage (StageKind kind, float amount, const std::vector<float>& in,
                                        double fs, int factor,
                                        dsp::IronModel ironModel = dsp::IronModel::classic)
    {
        const int n = static_cast<int> (in.size());
        std::vector<float> out (in.size());
        const int stages = factor == 8 ? 3 : factor == 4 ? 2 : factor == 2 ? 1 : 0;
        const double osRate = fs * factor;

        dsp::TubeStage tube;
        dsp::IronStage iron;
        tube.prepare (osRate);
        iron.prepare (osRate);
        tube.setAmount (amount);
        iron.setAmount (amount);
        iron.setModelImmediate (ironModel);

        auto processSample = [&] (float v)
        {
            return kind == StageKind::tube ? tube.process (v) : iron.process (v);
        };

        if (stages == 0)
        {
            for (int i = 0; i < n; ++i)
                out[static_cast<size_t> (i)] = processSample (in[static_cast<size_t> (i)]);
            return out;
        }

        dsp::Oversampler os;
        const int block = 256;
        os.prepare (block);
        os.setNumStages (stages);
        for (int start = 0; start < n; start += block)
        {
            const int len = std::min (block, n - start);
            float* hi = os.upsample (0, in.data() + start, len);
            for (int k = 0; k < len * factor; ++k)
                hi[k] = processSample (hi[k]);
            os.downsample (0, out.data() + start, len);
        }
        return out;
    }

    // Exact-bin frequency closest to `target` for an FFT of length n.
    inline double binFrequency (double target, double fs, int n)
    {
        const double bin = fs / n;
        return std::round (target / bin) * bin;
    }

    struct AliasReport
    {
        double worstAliasDb = -200.0;     // relative to the fundamental
        double worstAliasHz = 0.0;
    };

    // Measures non-harmonic content in 20 Hz .. 20 kHz (everything that is
    // not a harmonic of f0 or DC), relative to the fundamental.
    AliasReport measureAliasing (const float* x, int n, double fs, double f0);
}
