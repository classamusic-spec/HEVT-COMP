#pragma once

#include "DSP/EngineParams.h"
#include "DSP/Ballistics.h"
#include "Modes/ModeProfiles.h"

namespace heat::dsp
{
    // Result of the COMPRESS macro: the hidden expert parameters that the
    // single front-panel control drives.
    struct MacroResult
    {
        float thresholdDb = 0.0f;
        float ratio = 1.0f;
        float kneeDb = 6.0f;
        float colorDrive = 0.0f;
        float makeupFactor = 0.6f;
    };

    class CompressMacro
    {
    public:
        // Reference program level (dBFS, sine-referenced) used by the
        // conservative auto-makeup estimate.
        static constexpr float makeupReferenceDb = -12.0f;
        static constexpr float makeupLimitDb = 18.0f;

        static MacroResult evaluate (float compress, Mode mode) noexcept;

        // Timing for a given mode / detector / front-panel attack & release.
        static BallisticsSettings ballistics (Mode mode, Detector detector,
                                              float attackMs, float releaseMs,
                                              bool autoRelease) noexcept;

        // Static makeup estimate (dB, >= 0) for a curve.
        static float makeupDb (float thresholdDb, float ratio, float kneeDb, float factor) noexcept;
    };
}
