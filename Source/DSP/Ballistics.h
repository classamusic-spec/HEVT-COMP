#pragma once

#include "DSP/DspMath.h"

namespace heat::dsp
{
    // Timing description for the gain-reduction envelope. All times are in
    // milliseconds and converted to sample-rate-independent coefficients.
    struct BallisticsSettings
    {
        float attackMs = 3.0f;          // primary attack time constant
        float releaseMs = 250.0f;       // primary (fast-stage) release
        float roundingMs = 0.0f;        // second smoothing pole (attack rounding); 0 = off

        // Program dependence (dual-stage release with memory):
        float memoryWeight = 0.0f;      // 0 = pure single-stage release, 1 = full memory
        float memoryDepth = 0.5f;       // fraction of the current GR the memory can hold
        float memoryChargeMs = 500.0f;  // how long compression must last to fill the memory
        float memoryReleaseMs = 1500.0f;// slow-stage release

        // Level-dependent attack: attack accelerates by (1 + boost * excessDb)
        // when the requested reduction is much deeper than the current one.
        float attackBoostPerDb = 0.0f;
    };

    // Log-domain (dB) gain-reduction envelope.
    //
    //   hold   (r) : instant towards deeper reduction, releases towards the
    //                target with the release time. On periodic material this
    //                holds the per-cycle peak reduction, so the attack stage
    //                below sees a steady target and its time constant is
    //                accurate for any waveform.
    //   attack (y1): follows r with the attack time when r is deeper, and
    //                follows r directly while r is releasing. → attack and
    //                release are both exact and independent.
    //   memory (y2): slowly charges towards memoryDepth * y1 while compression
    //                is sustained; releases with memoryReleaseMs.
    //   output     : y1 + memoryWeight * min(0, y2 - y1)
    //                → after a short transient the memory is nearly empty and
    //                  recovery is fast; after sustained compression recovery
    //                  is fast at first, then slow (two-stage release).
    //   rounding   : optional second pole smoothing the result (rounded,
    //                less "edgy" attack onset).
    //
    // State and coefficients are double precision: at 192 kHz a 3 s time
    // constant needs (1 - a) ≈ 1.7e-6, which float cannot resolve accurately.
    // All values are gain reduction in dB (<= 0).
    class Ballistics
    {
    public:
        void prepare (double sampleRate) noexcept { fs = sampleRate; setSettings (settings); }

        void reset (float value = 0.0f) noexcept { r = y1 = y2 = yr = value; }

        void setSettings (const BallisticsSettings& s) noexcept;
        const BallisticsSettings& getSettings() const noexcept { return settings; }

        float process (float targetDb) noexcept
        {
            const double t = targetDb;

            // Hold / release stage.
            r = t < r ? t : r + kRelease * (t - r);

            // Attack stage.
            if (r < y1)
            {
                double k = kAttack;
                if (attackBoost > 0.0)
                    k = std::min (1.0, kAttack * (1.0 + attackBoost * (y1 - r)));
                y1 += k * (r - y1);
            }
            else
            {
                y1 = r;
            }

            // Memory stage.
            const double memTarget = memoryDepth * y1;
            y2 += (memTarget < y2 ? kMemCharge : kMemRelease) * (memTarget - y2);

            const double held = y1 + memoryWeight * std::min (0.0, y2 - y1);

            // Rounding pole.
            yr += kRounding * (held - yr);
            return static_cast<float> (yr);
        }

        float getCurrent() const noexcept { return static_cast<float> (yr); }
        float getMemory() const noexcept { return static_cast<float> (y2); }

    private:
        double fs = 48000.0;
        BallisticsSettings settings;
        // k = 1 - a (the fraction of the remaining distance covered per sample).
        double kAttack = 1.0, kRelease = 1.0, kRounding = 1.0, kMemCharge = 1.0, kMemRelease = 1.0;
        double memoryWeight = 0.0, memoryDepth = 0.5, attackBoost = 0.0;
        double r = 0.0, y1 = 0.0, y2 = 0.0, yr = 0.0;
    };
}
