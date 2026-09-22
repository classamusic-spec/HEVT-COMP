#pragma once

#include "Nonlinear/DCBlocker.h"

namespace heat::dsp
{
    // TUBE — original tube-inspired input colour stage (runs oversampled).
    //
    //   v      = drive * x + bias_dyn
    //   shaped = [ s(v) - s(bias_dyn) ] / (drive * s'(bias_dyn))
    //
    // s() is an asymmetric algebraic sigmoid (positive half saturates at +1,
    // negative half at -0.72), so the operating point `bias` sets the H2/H3
    // balance. The small-signal gain is normalised to exactly 1 at every
    // setting (built-in gain compensation), so TUBE never wins by being louder.
    //
    // Level dependence and memory:
    //   * bias_dyn = bias - k * env, env = 40 ms follower of |v| → loud passages
    //     shift the operating point (grid-conduction-like bias shift), changing
    //     the harmonic balance with a short memory.
    //   * The distortion component is DC-blocked (the linear part is untouched).
    //   * Subtle HF smoothing: blend towards an 18 kHz one-pole, scaled by amount.
    //
    // amount = 0 is an exact bypass; the stage converges continuously to the
    // identity as amount → 0 (the blend depth ramps in over the first 15 %).
    class TubeStage
    {
    public:
        void prepare (double oversampledRate) noexcept;
        void reset() noexcept;

        // Sets per-sample parameters from the amount (call at most once per
        // base-rate sample; cheap).
        void setAmount (float amount) noexcept;

        float process (float x) noexcept;

        float getAmount() const noexcept { return amount; }

        static constexpr float negativeCeiling = 0.72f;

    private:
        double fs = 96000.0;
        float amount = 0.0f;
        float drive = 1.0f, bias = 0.0f, depth = 0.0f, hfBlend = 0.0f;
        float envCoeff = 0.0f, env = 0.0f;
        DCBlocker dc;
        OnePoleLowpass hf;
    };
}
