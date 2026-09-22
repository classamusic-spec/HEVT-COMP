#pragma once

#include "DSP/EngineParams.h"
#include "Nonlinear/DCBlocker.h"
#include "Nonlinear/Hysteresis.h"

namespace heat::dsp
{
    // IRON — output transformer colour (runs oversampled).
    //
    // Both models share the mechanism by which a transformer distorts: the core
    // flux is the time integral of the voltage, so the colouring is naturally
    // low-frequency dependent, and the distortion appears as the voltage the
    // nonlinear magnetising current drops across the source impedance:
    //
    //   flux      φ  = leaky ∫ x dt   (normalised so |φ| ≈ |x| at 50 Hz,
    //                                  leak at 8 Hz keeps it bounded)
    //
    // HYSTERESIS (default since 2.1) — inverse Jiles–Atherton core:
    //   B = drive · φ,  H = JA(B)  (history-dependent, see Hysteresis.h)
    //   e = (μi · H − B) / drive   the magnetising current minus its linear
    //                              (inductive) part, in flux units
    //   distortion = −depth · e
    //   → Rayleigh-type minor loops at low level (distortion falls only in
    //     proportion to level), a real B–H loop (level-dependent LF phase),
    //     and a steep saturation knee at high LF levels.
    //
    // CLASSIC (HEAT 2.0, kept so older sessions sound identical):
    //   memory       : φ_h = φ − hc · sigmoid(dφ/dt)  (narrow hysteresis-like loop)
    //   saturation   : e = k · softCubic(φ_h)
    //
    // Shared: the distortion component is DC-blocked; an additive first-order
    // low shelf adds weight (+1.6 dB · amount, 80 Hz) and a one-pole HF
    // softening blends in with amount. amount = 0 is an exact bypass. Model
    // changes cross-fade the distortion components over 20 ms.
    class IronStage
    {
    public:
        void prepare (double oversampledRate) noexcept;
        void reset() noexcept;
        void setAmount (float amount) noexcept;
        void setModel (IronModel model) noexcept;
        // Snaps the model cross-fade (prepare / state load).
        void setModelImmediate (IronModel model) noexcept;
        float process (float x) noexcept;

        float getAmount() const noexcept { return amount; }
        const JilesAthertonCore& getCore() const noexcept { return core; }

        // Hysteresis mapping (exposed for measurements).
        static double coreDriveFor (float amount) noexcept { return 1.0 + 3.0 * amount; }
        static double coreDepthFor (float amount) noexcept { return 0.055 * amount; }

    private:
        double fs = 96000.0;
        float amount = 0.0f, depth = 0.0f, k = 0.0f, hc = 0.0f, shelfGain = 0.0f, hfBlend = 0.0f;

        // Flux integrator (double precision: tiny increments at high rates).
        double flux = 0.0, fluxLeak = 0.0, fluxGain = 0.0;
        float prevFlux = 0.0f;
        float slopeScale = 1.0f;

        // Hysteresis core.
        JilesAthertonCore core;
        double coreDrive = 1.0, coreDepth = 0.0, coreMu = 1.0;

        // Model cross-fade: 0 = classic, 1 = hysteresis.
        float modelWeight = 0.0f, modelTarget = 0.0f, modelStep = 0.0f;

        // Low-shelf low-pass (one pole, fixed cutoff).
        float shelfCoeff = 0.0f, shelfState = 0.0f;

        DCBlocker dc;
        OnePoleLowpass hf;
    };
}
