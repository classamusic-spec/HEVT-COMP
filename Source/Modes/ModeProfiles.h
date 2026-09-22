#pragma once

#include "DSP/EngineParams.h"

namespace heat::dsp
{
    // Five anchors at COMPRESS = 0, 25, 50, 75, 100 %. Interpolated with a
    // monotone cubic (Fritsch–Carlson), so a monotone anchor list is a
    // monotone curve: no overshoot, no unexpected jumps.
    struct MacroCurve
    {
        float anchors[5];

        float evaluate (float compress) const noexcept;
    };

    // Everything that makes CLEAN, WARM and DRIVE behave differently.
    // These are real DSP differences, not presets.
    struct ModeProfile
    {
        const char* name;

        // COMPRESS macro curves.
        MacroCurve thresholdDb;
        MacroCurve ratio;
        MacroCurve kneeDb;

        // Timing character.
        float attackScale;          // multiplies the front-panel attack
        float releaseScale;         // multiplies the front-panel release
        float roundingFraction;     // rounding pole = roundingFraction * attack (+ roundingMinMs)
        float roundingMinMs;

        // Program dependence.
        float memoryWeight;
        float memoryDepth;
        float memoryChargeMs;
        float memoryReleaseMult;    // slow stage = release * mult

        // Detector character: blends the PEAK reading towards the RMS reading
        // ("soft detector").
        float detectorSoftening;

        // Conservative auto-makeup: fraction of the static GR at the reference
        // program level that is given back.
        float makeupFactor;

        // Built-in colour stage (the mode's own amplifier character).
        MacroCurve colorDrive;      // 0..1 saturation drive vs COMPRESS
        float colorGrInteraction;   // extra drive at full reduction (scaled by 1 - linear GR)
        float colorBias;            // asymmetry (even harmonics)
    };

    struct DetectorProfile
    {
        const char* name;
        float attackScale;
        float attackFloorMs;        // the detector can never attack faster than this
        float releaseScale;
        float attackBoostPerDb;
        float memoryWeight;
        float memoryDepth;
        float memoryChargeMs;
        float memoryReleaseMult;
    };

    const ModeProfile& getModeProfile (Mode mode) noexcept;
    const DetectorProfile& getDetectorProfile (Detector detector) noexcept;
}
