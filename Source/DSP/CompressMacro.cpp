#include "DSP/CompressMacro.h"
#include "DSP/GainComputer.h"

#include <algorithm>

namespace heat::dsp
{
    MacroResult CompressMacro::evaluate (float compress, Mode mode) noexcept
    {
        const auto& p = getModeProfile (mode);
        const float c = clamp01 (compress);

        MacroResult r;
        r.thresholdDb = p.thresholdDb.evaluate (c);
        r.ratio = std::max (1.0f, p.ratio.evaluate (c));
        r.kneeDb = std::max (0.0f, p.kneeDb.evaluate (c));
        r.colorDrive = clamp01 (p.colorDrive.evaluate (c));
        r.makeupFactor = p.makeupFactor;
        return r;
    }

    BallisticsSettings CompressMacro::ballistics (Mode mode, Detector detector,
                                                  float attackMs, float releaseMs,
                                                  bool autoRelease) noexcept
    {
        const auto& m = getModeProfile (mode);
        const auto& d = getDetectorProfile (detector);

        BallisticsSettings s;
        s.attackMs = std::max (attackMs * m.attackScale * d.attackScale, d.attackFloorMs);
        s.releaseMs = releaseMs * m.releaseScale * d.releaseScale;
        s.roundingMs = m.roundingFraction > 0.0f ? m.roundingMinMs + m.roundingFraction * attackMs : 0.0f;
        s.attackBoostPerDb = d.attackBoostPerDb;

        // Program dependence: the stronger of the mode's and the detector's
        // memory character wins; AUTO RELEASE forces a strong memory in any mode.
        const bool detectorDominates = d.memoryWeight > m.memoryWeight;
        s.memoryWeight = std::max (m.memoryWeight, d.memoryWeight);
        s.memoryDepth = detectorDominates ? d.memoryDepth : m.memoryDepth;
        s.memoryChargeMs = detectorDominates ? d.memoryChargeMs : m.memoryChargeMs;
        const float mult = detectorDominates ? d.memoryReleaseMult : m.memoryReleaseMult;
        s.memoryReleaseMs = releaseMs * m.releaseScale * mult;

        if (autoRelease)
        {
            s.memoryWeight = std::max (s.memoryWeight, 0.9f);
            s.memoryDepth = std::max (s.memoryDepth, 0.6f);
            s.memoryReleaseMs = std::max (s.memoryReleaseMs, releaseMs * 5.0f);
            s.releaseMs = std::min (s.releaseMs, releaseMs * 0.5f);
        }
        return s;
    }

    float CompressMacro::makeupDb (float thresholdDb, float ratio, float kneeDb, float factor) noexcept
    {
        const float gr = GainComputer::gainReductionDb (makeupReferenceDb,
                                                        GainComputerSettings { thresholdDb, ratio, kneeDb });
        return std::clamp (-gr * factor, 0.0f, makeupLimitDb);
    }
}
