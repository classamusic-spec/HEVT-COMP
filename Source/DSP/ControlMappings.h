#pragma once

#include <cmath>

// Front-panel ↔ internal mappings, shared by the parameters, the DSP tests
// and the UI. Attack, release and HPF are logarithmic so the common musical
// ranges get most of the knob travel:
//
//   ATTACK   0.1 ms … 100 ms   (noon ≈ 3.2 ms)
//   RELEASE  20 ms  … 3000 ms  (noon ≈ 245 ms)
//   HPF      20 Hz  … 400 Hz   (noon ≈ 89 Hz)

namespace heat::dsp
{
    inline constexpr float attackMinMs = 0.1f, attackMaxMs = 100.0f;
    inline constexpr float releaseMinMs = 20.0f, releaseMaxMs = 3000.0f;
    inline constexpr float hpfMinHz = 20.0f, hpfMaxHz = 400.0f;

    inline float logMap (float minV, float maxV, float normalised) noexcept
    {
        return minV * std::pow (maxV / minV, normalised);
    }

    inline float logUnmap (float minV, float maxV, float value) noexcept
    {
        return std::log (value / minV) / std::log (maxV / minV);
    }

    inline float attackFromNormalised (float n) noexcept  { return logMap (attackMinMs, attackMaxMs, n); }
    inline float releaseFromNormalised (float n) noexcept { return logMap (releaseMinMs, releaseMaxMs, n); }
    inline float hpfFromNormalised (float n) noexcept     { return logMap (hpfMinHz, hpfMaxHz, n); }
}
