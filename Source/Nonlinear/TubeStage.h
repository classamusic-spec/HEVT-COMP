#pragma once

#include "Nonlinear/DCBlocker.h"

namespace heat::dsp
{
    // TUBE — original tube-inspired input colour stage (runs oversampled).
    //
    //   v      = drive * x
    //   shaped = [ s(v + b) - s(b) ] / (drive * s'(b))
    //
    // s(u) = u / sqrt(1 + u^2) is analytic (smooth at every derivative order),
    // so the harmonic series decays quickly and oversampling is effective.
    // Running it off-centre at the operating point b makes it asymmetric: b
    // sets the even-harmonic content (H2) while drive sets how far into the
    // curve the signal reaches (H3 and above). The small-signal gain is
    // normalised to exactly 1 at every setting (built-in gain compensation), so
    // TUBE never wins by being louder.
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

    private:
        double fs = 96000.0;
        float amount = 0.0f;
        float drive = 1.0f, bias = 0.0f, depth = 0.0f, hfBlend = 0.0f;
        float envCoeff = 0.0f, env = 0.0f;
        DCBlocker dc;
        OnePoleLowpass hf;
    };
}
