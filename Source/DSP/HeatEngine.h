#pragma once

#include "DSP/CompressorEngine.h"
#include "DSP/DryWetAligner.h"
#include "DSP/EngineParams.h"
#include "DSP/Multiband.h"
#include "DSP/SidechainFilter.h"
#include "DSP/TruePeakLimiter.h"
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

        // 2.1
        float limiterGrDb = 0.0f;          // deepest limiter reduction during the block
        float bandGrDb[3] { 0.0f, 0.0f, 0.0f }; // deepest reduction per band (LOW / MID / HIGH)
        bool multibandActive = false;
        bool midSideActive = false;
        int latencySamples = 0;
    };

    // The complete HEAT signal chain (see docs/DSP_ARCHITECTURE.md):
    //
    //   in ─► INPUT ─┬──────────────────────────────────────────────► BYPASS (aligned)
    //                ├─► SIDECHAIN (int/ext) ─► HPF / LPF / BELL ─┬─► [L/R→M/S] ─► COMPRESSOR ──────────┐
    //                │                                            │              └► BAND SPLIT ─► BANDS ─┤ gains
    //                │                                            └─► SC LISTEN (aligned)                 │
    //                └─► LOOKAHEAD ─┬─► [L/R→M/S] ─► WET: [↑OS] TUBE ─► ×gain (+ band shelves) ◄──────────┘
    //                               │                      ─► MODE COLOUR ─► IRON [↓OS]
    //                               │                      (or a delay path when no colour is active)
    //                               └─► DRY (aligned)
    //   [M/S→L/R] ─► MIX ─► LISTEN ─► OUTPUT ─► LIMITER ─► BYPASS X-FADE ─► SAFETY ─► out
    //
    // Latency = core (constant; independent of TUBE / IRON / MODE / QUALITY /
    // MULTIBAND / STEREO MODE) + LOOKAHEAD + limiter look-ahead when the LIMITER
    // is on. Latency changes are cross-faded on every path.
    class HeatEngine
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int chunkSize = 256;
        static constexpr float maxLookaheadMs = 10.0f;
        static constexpr float latencyFadeMs = 20.0f;

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset() noexcept;

        void setParams (const EngineParams& p) noexcept { params = p; }
        // Snaps all smoothers to the current parameters (state load / prepare).
        void snapToParams() noexcept;

        // io: numChannels in/out buffers. sidechain may be nullptr.
        void process (float* const* io, int numChannels,
                      const float* const* sidechain, int numSidechainChannels,
                      int numSamples) noexcept;

        // Latency for the current parameters (what the host must be told).
        int getLatencySamples() const noexcept { return latencyFor (params.lookaheadMs, params.limiter); }
        // Latency for a given LOOKAHEAD / LIMITER setting at the prepared rate.
        int latencyFor (float lookaheadMs, bool limiterOn) const noexcept;
        int getCoreLatencySamples() const noexcept { return coreLatency; }
        int getLimiterLatencySamples() const noexcept { return limiter.getLatency(); }

        const EngineTelemetry& getTelemetry() const noexcept { return telemetry; }
        double getSampleRate() const noexcept { return fs; }

        // Test hooks.
        bool isColourPathRunning() const noexcept { return colourState != ColourState::off; }
        bool isMultibandRunning() const noexcept { return mbState != MbState::off; }
        int stagesForQuality (Quality q) const noexcept;

        // Processing-domain rotation: t = 0 → L/R, t = 1 → M/S (M = (L+R)/2,
        // S' = (R−L)/2). Orthogonal up to scale for every t, so the morph
        // between the two is continuous and always invertible.
        struct StereoMatrix
        {
            float e00 = 1.0f, e01 = 0.0f, tanTheta = 0.0f;
            static StereoMatrix forMorph (float t) noexcept;
            void encode (float l, float r, float& a, float& b) const noexcept
            {
                a = e00 * l + e01 * r;
                b = e00 * r - e01 * l;
            }
            void decode (float a, float b, float& l, float& r) const noexcept
            {
                l = a - tanTheta * b;
                r = b + tanTheta * a;
            }
        };

    private:
        enum class ColourState { off, priming, fadingIn, on, fadingOut };
        enum class MbState { off, priming, fadingIn, on, fadingOut };

        void processChunk (float* const* io, int numCh, const float* const* sidechain, int numSc,
                           int offset, int n) noexcept;
        void updateColourState (int n) noexcept;
        void startColourPath() noexcept;
        bool wantsColour() const noexcept;
        void configureStages (int stages) noexcept;
        void runColourPath (int numCh, int n) noexcept;

        void updateMultibandState (int n) noexcept;
        void startMultiband() noexcept;
        void setBandControls (int blockSize, bool snap) noexcept;
        void computeGains (int numCh, int n) noexcept;

        int lookaheadSamplesFor (float ms) const noexcept;

        double fs = 48000.0;
        int preparedChannels = 2;
        int coreLatency = 0;
        EngineParams params;
        EngineTelemetry telemetry;

        CompressorEngine compressor;
        SidechainFilter scFilter;
        Oversampler oversampler;
        TubeStage tube[maxChannels];
        IronStage iron[maxChannels];
        ModeColor colour[maxChannels];
        TruePeakLimiter limiter;

        LinearSmoother inputGainDb, outputGainDb, mix, scExternal, listen, bypass, limiterEnable;
        OnePoleSmoother tubeAmount, ironAmount;

        // Stereo processing domain.
        LinearSmoother stereoMorph, processAmount[maxChannels];
        DelayLine morphDelay, processDelay[maxChannels];

        CrossfadeDelay bypassLine[maxChannels], lookaheadLine[maxChannels], listenLine[maxChannels], limiterLine[maxChannels];
        DelayLine dryDelay[maxChannels], gainDelay[maxChannels];
        DelayLine osGainDelay[maxChannels], osPad[maxChannels], colourDriveDelay[maxChannels], colourBiasDelay[maxChannels];

        // Multiband.
        BandSplitter splitter;
        CompressorEngine bandComp[3];
        MbState mbState = MbState::off;
        MultibandMode mbStructure = MultibandMode::off;
        LinearSmoother mbWeight;
        int mbCounter = 0;
        DelayLine lowRatioDelay[maxChannels], highRatioDelay[maxChannels];     // base path (core latency)
        DelayLine osLowRatioDelay[maxChannels], osHighRatioDelay[maxChannels]; // colour path (gain alignment)
        ShelfState baseLow[maxChannels], baseHigh[maxChannels], osLow[maxChannels], osHigh[maxChannels];
        ShelfCoefficients osLowPrev[maxChannels], osHighPrev[maxChannels];
        float shelfG0Base[2] { 0.0f, 0.0f }, shelfG0Os[2] { 0.0f, 0.0f };

        // Per-quality latency bookkeeping.
        int stagesPerQuality[3] { 1, 2, 3 };
        int padPerStages[4] {};        // high-rate pad samples, indexed by stage count
        int gainDelayPerStages[4] {};  // high-rate gain delay, indexed by stage count
        int driveDelayPerStages[4] {}; // base-rate colour-drive delay (also band shelf ratios)

        ColourState colourState = ColourState::off;
        int colourCounter = 0;
        int fadeLength = 512;
        int activeStages = 1;
        float lastGainLin[maxChannels] { 1.0f, 1.0f };

        // Scratch (chunkSize each).
        std::vector<float> pre[maxChannels], aud[maxChannels], audP[maxChannels], dry[maxChannels],
                           sc[maxChannels], scP[maxChannels], gr[maxChannels], grEff[maxChannels],
                           gainLin[maxChannels], gainDelayed[maxChannels], wetOs[maxChannels],
                           bypassed[maxChannels], listenSig[maxChannels], drive[maxChannels],
                           lowRatio[maxChannels], highRatio[maxChannels], lowRatioD[maxChannels], highRatioD[maxChannels],
                           osLowRatio[maxChannels], osHighRatio[maxChannels], pIn[maxChannels], pOut[maxChannels];
        std::vector<float> bandSc[3][maxChannels], bandGr[3][maxChannels], bandMakeup[3], mbW;
        std::vector<float> makeup, colourDrive, colourBias, colourInteraction, tubeA, ironA, tIn, tOut;
        bool msInChunk = false, msOutChunk = false, mbInChunk = false, limiterRunning = false;
    };
}
