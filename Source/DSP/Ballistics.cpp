#include "DSP/Ballistics.h"

namespace heat::dsp
{
    namespace
    {
        // 1 - exp(-1 / (tau * fs)), computed without cancellation.
        double stepFraction (double timeMs, double fs) noexcept
        {
            const double samples = std::max (1.0e-3, timeMs * 1.0e-3 * fs);
            return -std::expm1 (-1.0 / samples);
        }
    }

    void Ballistics::setSettings (const BallisticsSettings& s) noexcept
    {
        settings = s;
        kAttack = stepFraction (std::max (0.01f, s.attackMs), fs);
        kRelease = stepFraction (std::max (1.0f, s.releaseMs), fs);
        kRounding = s.roundingMs > 0.001f ? stepFraction (s.roundingMs, fs) : 1.0;
        kMemCharge = stepFraction (std::max (1.0f, s.memoryChargeMs), fs);
        kMemRelease = stepFraction (std::max (1.0f, s.memoryReleaseMs), fs);
        memoryWeight = clamp01 (s.memoryWeight);
        memoryDepth = clamp01 (s.memoryDepth);
        attackBoost = std::max (0.0f, s.attackBoostPerDb);
    }
}
