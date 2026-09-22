#pragma once

#include "DSP/Ballistics.h"
#include "DSP/CompressMacro.h"
#include "DSP/DspMath.h"
#include "DSP/EngineParams.h"
#include "DSP/OpticalDetector.h"
#include "DSP/PeakDetector.h"
#include "DSP/RMSDetector.h"

namespace heat::dsp
{
    // Sidechain → detector → stereo link → gain computer → ballistics.
    //
    // Produces, per sample: the gain reduction for each channel (dB, <= 0), the
    // auto-makeup gain (dB, >= 0) and the mode's colour drive. It never touches
    // the audio path itself; HeatEngine applies the gain where the topology
    // requires it (possibly inside the oversampled colour path).
    class CompressorEngine
    {
    public:
        static constexpr int maxChannels = 2;

        struct Controls
        {
            float compress = 0.5f;
            Mode mode = Mode::warm;
            Detector detector = Detector::peak;
            float attackMs = 3.0f;
            float releaseMs = 250.0f;
            StereoLink link = StereoLink::linked;
            bool autoRelease = false;
            bool autoMakeup = true;
        };

        struct Outputs
        {
            float* grDb[maxChannels] {};   // per-channel gain reduction
            float* makeupDb = nullptr;      // shared makeup gain
            float* colorDrive = nullptr;    // shared colour drive (0..1)
            float* colorBias = nullptr;     // shared colour asymmetry
            float* colorInteraction = nullptr; // extra colour drive at full reduction
        };

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        // Called once per block from the audio thread. Values are smoothed
        // internally (sample-accurate for the curve, block-rate for timing).
        void setControls (const Controls& c, int blockSize) noexcept;

        // Snaps every smoother to the current controls (no glide).
        void snapToControls (const Controls& c) noexcept;

        void process (const float* const* sidechain, int numChannels, int numSamples, const Outputs& out) noexcept;

        // Telemetry (audio thread writes, read via HeatEngine).
        float getEffectiveThresholdDb() const noexcept { return thr.getCurrent(); }
        float getEffectiveRatio() const noexcept;
        float getEffectiveKneeDb() const noexcept { return knee.getCurrent(); }
        float getColorBias() const noexcept { return colorBias.getCurrent(); }
        float getColorGrInteraction() const noexcept { return colorGrInteraction.getCurrent(); }
        const BallisticsSettings& getBallisticsSettings() const noexcept { return timing; }

    private:
        void applyTargets (const Controls& c) noexcept;

        double fs = 48000.0;
        Controls controls;

        PeakDetector peak[maxChannels];
        RMSDetector rms[maxChannels];
        OpticalDetector optical[maxChannels];
        Ballistics ballistics[maxChannels];

        // Curve smoothing (per sample).
        OnePoleSmoother thr, slope, knee, makeupFactor, colorDrive, colorGrInteraction, colorBias, softening;
        LinearSmoother detWeight[numDetectors];
        LinearSmoother linkAmount;

        // Timing smoothing (per block).
        BallisticsSettings timing, timingTarget;
    };
}
