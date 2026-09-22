#pragma once

#include "DSP/CompressorEngine.h"
#include "DSP/DryWetAligner.h"
#include "DSP/EngineParams.h"
#include "DSP/SidechainFilter.h"
#include "Nonlinear/IronStage.h"
#include "Nonlinear/ModeColor.h"
#include "Nonlinear/TubeStage.h"
#include "Quality/Oversampler.h"

#include <vector>

namespace heat::dsp
{
    struct EngineTelemetry
    {
        float grDb[2] { 0.0f, 0.0f };      // deepest reduction during the last block (dB, <= 0)
        float grDbLast[2] { 0.0f, 0.0f };  // reduction at the end of the last block
        float makeupDb = 0.0f;
        float thresholdDb = 0.0f;
        float ratio = 1.0f;
        float kneeDb = 0.0f;
        float attackMs = 0.0f;
        float releaseMs = 0.0f;
        float inputPeak = 0.0f;
        float outputPeak = 0.0f;
        bool colourPathActive = false;
        int oversamplingFactor = 1;
        bool recoveredFromNonFinite = false;
    };

    // The complete HEAT signal chain (see docs/DSP_ARCHITECTURE.md):
    //
    //   in ─► INPUT GAIN ─┬───────────────────────────► DRY (latency-aligned)
    //                     ├─► SIDECHAIN (int/ext) ─► SC HPF ─► COMPRESSOR ─► gain
    //                     └─► WET: [↑OS] TUBE ─► ×gain ─► MODE COLOUR ─► IRON [↓OS]
    //                                         (or a pure delay path when no colour is active)
    //   WET/DRY ─► MIX ─► (SC LISTEN) ─► OUTPUT GAIN ─► BYPASS X-FADE ─► SAFETY ─► out
    //
    // Latency is constant (independent of TUBE / IRON / MODE / QUALITY) and
    // is reported once from prepare().
    class HeatEngine
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int chunkSize = 256;

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset() noexcept;

        void setParams (const EngineParams& p) noexcept { params = p; }
        // Snaps all smoothers to the current parameters (state load / prepare).
        void snapToParams() noexcept;

        // io: numChannels in/out buffers. sidechain may be nullptr.
        void process (float* const* io, int numChannels,
                      const float* const* sidechain, int numSidechainChannels,
                      int numSamples) noexcept;

        int getLatencySamples() const noexcept { return latency; }
        const EngineTelemetry& getTelemetry() const noexcept { return telemetry; }
        double getSampleRate() const noexcept { return fs; }

        // Test hooks.
        bool isColourPathRunning() const noexcept { return colourState != ColourState::off; }
        int stagesForQuality (Quality q) const noexcept;

    private:
        enum class ColourState { off, priming, fadingIn, on, fadingOut };

        void processChunk (float* const* io, int numCh, const float* const* sidechain, int numSc,
                           int offset, int n) noexcept;
        void updateColourState (int n) noexcept;
        void startColourPath() noexcept;
        bool wantsColour() const noexcept;
        void configureStages (int stages) noexcept;
        void runColourPath (int numCh, int n) noexcept;

        double fs = 48000.0;
        int preparedChannels = 2;
        int latency = 0;
        EngineParams params;
        EngineTelemetry telemetry;

        CompressorEngine compressor;
        SidechainFilter scFilter;
        Oversampler oversampler;
        TubeStage tube[maxChannels];
        IronStage iron[maxChannels];
        ModeColor colour[maxChannels];

        LinearSmoother inputGainDb, outputGainDb, mix, scExternal, listen, bypass;
        OnePoleSmoother tubeAmount, ironAmount;

        DelayLine bypassDelay[maxChannels], dryDelay[maxChannels], gainDelay[maxChannels], listenDelay[maxChannels];
        DelayLine osGainDelay[maxChannels], osPad[maxChannels], colourDriveDelay[maxChannels], colourBiasDelay[maxChannels];

        // Per-quality latency bookkeeping.
        int stagesPerQuality[3] { 1, 2, 3 };
        int padPerStages[4] {};        // high-rate pad samples, indexed by stage count
        int gainDelayPerStages[4] {};  // high-rate gain delay, indexed by stage count
        int driveDelayPerStages[4] {}; // base-rate colour-drive delay

        ColourState colourState = ColourState::off;
        int colourCounter = 0;
        int fadeLength = 512;
        int activeStages = 1;
        float lastGainLin[maxChannels] { 1.0f, 1.0f };

        // Scratch (chunkSize each).
        std::vector<float> pre[maxChannels], dry[maxChannels], sc[maxChannels], gr[maxChannels],
                           gainLin[maxChannels], gainDelayed[maxChannels], wetOs[maxChannels],
                           bypassed[maxChannels], listenSig[maxChannels], drive[maxChannels];
        std::vector<float> makeup, colourDrive, colourBias, colourInteraction, tubeA, ironA;
    };
}
