#pragma once

#include "Nonlinear/DCBlocker.h"

namespace heat::dsp
{
    // IRON — transformer-inspired output weight (runs oversampled).
    //
    // Not a physical transformer model. It borrows the mechanism by which a
    // transformer distorts — core flux is the time integral of the voltage —
    // so the colouring is naturally low-frequency dependent:
    //
    //   flux      φ  = leaky ∫ x dt   (normalised so |φ| ≈ |x| at 50 Hz,
    //                                  leak at 8 Hz keeps it bounded)
    //   memory       : φ_h = φ - hc * sigmoid(dφ/dt)  (direction-dependent
    //                  offset → a narrow hysteresis-like loop)
    //   saturation   : e = k * softCubic(φ_h)  → mostly odd harmonics that grow
    //                  as frequency falls and level rises
    //   weight       : additive first-order low shelf (+1.6 dB * amount,
    //                  corner 80 Hz), x + (G-1) * LP1(x), modulation-safe
    //   HF softening : blend towards a 24 kHz one-pole, scaled by amount
    //
    // The distortion component is DC-blocked; amount = 0 is an exact bypass.
    class IronStage
    {
    public:
        void prepare (double oversampledRate) noexcept;
        void reset() noexcept;
        void setAmount (float amount) noexcept;
        float process (float x) noexcept;

        float getAmount() const noexcept { return amount; }

    private:
        double fs = 96000.0;
        float amount = 0.0f, depth = 0.0f, k = 0.0f, hc = 0.0f, shelfGain = 0.0f, hfBlend = 0.0f;

        // Flux integrator (double precision: tiny increments at high rates).
        double flux = 0.0, fluxLeak = 0.0, fluxGain = 0.0;
        float prevFlux = 0.0f;
        float slopeScale = 1.0f;

        // Low-shelf low-pass (one pole, fixed cutoff).
        float shelfCoeff = 0.0f, shelfState = 0.0f;

        DCBlocker dc;
        OnePoleLowpass hf;
    };
}
