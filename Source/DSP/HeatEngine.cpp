#include "DSP/HeatEngine.h"
#include "DSP/ControlMappings.h"

#include <cmath>

namespace heat::dsp
{
    namespace
    {
        constexpr float gainSmoothingMs = 20.0f;
        constexpr float mixSmoothingMs = 20.0f;
        constexpr float amountSmoothingMs = 30.0f;
        constexpr float sourceFadeMs = 20.0f;
        constexpr float listenFadeMs = 15.0f;
        constexpr float bypassFadeMs = 20.0f;
        constexpr float stereoMorphMs = 30.0f;
        constexpr float multibandPrimeMs = 25.0f;
        constexpr float multibandFadeMs = 30.0f;
        constexpr float safetyCeiling = 31.6f; // ≈ +30 dBFS: never reached by sane material

        // Band timing relative to the front panel: lows breathe slower, highs
        // recover faster (less audible pumping in either band).
        constexpr float bandAttackScale[3]  { 1.5f, 1.0f, 0.6f };
        constexpr float bandReleaseScale[3] { 1.5f, 1.0f, 0.6f };

        CompressorEngine::Controls controlsFrom (const EngineParams& p) noexcept
        {
            CompressorEngine::Controls c;
            c.compress = clamp01 (p.compress);
            c.mode = p.mode;
            c.detector = p.detector;
            c.attackMs = std::clamp (p.attackMs, attackMinMs, attackMaxMs);
            c.releaseMs = std::clamp (p.releaseMs, releaseMinMs, releaseMaxMs);
            c.link = p.link;
            c.autoRelease = p.autoRelease;
            c.autoMakeup = p.autoMakeup;
            c.linkInclude[0] = p.stereoMode != StereoMode::sideOnly;
            c.linkInclude[1] = p.stereoMode != StereoMode::midOnly;
            return c;
        }

        int filterLatencyAtTopRate (int stages)
        {
            const int factor = 1 << stages;
            int latency = 0;
            for (int s = 0; s < stages; ++s)
                latency += ((Oversampler::stageTaps[s] - 1) / 2) * (factor >> (s + 1));
            return latency;
        }

        float morphTargetFor (StereoMode m) noexcept { return m == StereoMode::leftRight ? 0.0f : 1.0f; }
    }

    HeatEngine::StereoMatrix HeatEngine::StereoMatrix::forMorph (float t) noexcept
    {
        StereoMatrix m;
        if (t <= 0.0f)
            return m; // exact identity
        if (t >= 1.0f)
        {
            m.e00 = 0.5f;
            m.e01 = 0.5f;
            m.tanTheta = 1.0f;
            return m;
        }
        const double theta = 0.25 * pi * static_cast<double> (t);
        const double c = std::cos (theta), s = std::sin (theta);
        m.e00 = static_cast<float> (c * c);
        m.e01 = static_cast<float> (c * s);
        m.tanTheta = static_cast<float> (s / c);
        return m;
    }

    int HeatEngine::stagesForQuality (Quality q) const noexcept
    {
        return stagesPerQuality[std::clamp (static_cast<int> (q), 0, 2)];
    }

    int HeatEngine::lookaheadSamplesFor (float ms) const noexcept
    {
        return static_cast<int> (std::lround (std::clamp (ms, 0.0f, maxLookaheadMs) * 1.0e-3 * fs));
    }

    int HeatEngine::latencyFor (float lookaheadMs, bool limiterOn) const noexcept
    {
        return coreLatency + lookaheadSamplesFor (lookaheadMs) + (limiterOn ? limiter.getLatency() : 0);
    }

    void HeatEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        (void) maxBlockSize; // the engine processes in fixed chunks of chunkSize
        fs = sampleRate;
        preparedChannels = std::clamp (numChannels, 1, maxChannels);

        compressor.prepare (fs);
        for (auto& b : bandComp)
            b.prepare (fs);
        scFilter.prepare (fs);
        splitter.prepare (fs);
        oversampler.prepare (chunkSize);
        limiter.prepare (fs);

        // Above 100 kHz the host rate already provides headroom for harmonics:
        // drop one 2x stage (two above 150 kHz) to keep CPU sane.
        const int reduction = fs > 150000.0 ? 2 : fs > 100000.0 ? 1 : 0;
        for (int q = 0; q < 3; ++q)
            stagesPerQuality[q] = std::max (1, q + 1 - reduction);

        const int maxStagesUsed = stagesPerQuality[2];
        coreLatency = static_cast<int> (std::ceil (oversampler.getRoundTripLatencyBaseRate (maxStagesUsed) - 1.0e-9));

        int maxPad = 0, maxGainDelay = 0, maxDriveDelay = 0;
        for (int s = 1; s <= Oversampler::maxStages; ++s)
        {
            const int factor = 1 << s;
            const int roundTripHigh = static_cast<int> (std::lround (oversampler.getRoundTripLatencyBaseRate (s) * factor));
            padPerStages[s] = std::max (0, coreLatency * factor - roundTripHigh);
            const int upLatency = filterLatencyAtTopRate (s);
            gainDelayPerStages[s] = std::max (0, upLatency - (factor - 1));
            driveDelayPerStages[s] = (upLatency + factor / 2) / factor;
            maxPad = std::max (maxPad, padPerStages[s]);
            maxGainDelay = std::max (maxGainDelay, gainDelayPerStages[s]);
            maxDriveDelay = std::max (maxDriveDelay, driveDelayPerStages[s]);
        }

        const int maxLookahead = lookaheadSamplesFor (maxLookaheadMs);
        const int limiterLatency = limiter.getLatency();
        const int latencyFade = std::max (1, static_cast<int> (latencyFadeMs * 1.0e-3 * fs));

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            bypassLine[ch].prepare (coreLatency + maxLookahead + limiterLatency, latencyFade);
            lookaheadLine[ch].prepare (maxLookahead, latencyFade);
            listenLine[ch].prepare (coreLatency + maxLookahead, latencyFade);
            limiterLine[ch].prepare (limiterLatency, latencyFade);

            for (auto* d : { &dryDelay[ch], &gainDelay[ch], &lowRatioDelay[ch], &highRatioDelay[ch], &processDelay[ch] })
            {
                d->prepare (coreLatency);
                d->setDelay (coreLatency);
            }
            osGainDelay[ch].prepare (maxGainDelay);
            osPad[ch].prepare (maxPad);
            colourDriveDelay[ch].prepare (maxDriveDelay);
            colourBiasDelay[ch].prepare (maxDriveDelay);
            osLowRatioDelay[ch].prepare (maxDriveDelay);
            osHighRatioDelay[ch].prepare (maxDriveDelay);

            for (auto* v : { &pre[ch], &aud[ch], &audP[ch], &dry[ch], &sc[ch], &scP[ch], &gr[ch], &grEff[ch],
                             &gainLin[ch], &gainDelayed[ch], &wetOs[ch], &bypassed[ch], &listenSig[ch], &drive[ch],
                             &lowRatio[ch], &highRatio[ch], &lowRatioD[ch], &highRatioD[ch],
                             &osLowRatio[ch], &osHighRatio[ch], &pIn[ch], &pOut[ch] })
                v->assign (chunkSize, 0.0f);
            for (int b = 0; b < 3; ++b)
            {
                bandSc[b][ch].assign (chunkSize, 0.0f);
                bandGr[b][ch].assign (chunkSize, 0.0f);
            }
        }
        morphDelay.prepare (coreLatency);
        morphDelay.setDelay (coreLatency);
        for (auto& v : bandMakeup)
            v.assign (chunkSize, 0.0f);
        for (auto* v : { &makeup, &colourDrive, &colourBias, &colourInteraction, &tubeA, &ironA, &tIn, &tOut, &mbW })
            v->assign (chunkSize, 0.0f);

        inputGainDb.prepare (fs, gainSmoothingMs);
        outputGainDb.prepare (fs, gainSmoothingMs);
        mix.prepare (fs, mixSmoothingMs);
        scExternal.prepare (fs, sourceFadeMs);
        listen.prepare (fs, listenFadeMs);
        bypass.prepare (fs, bypassFadeMs);
        limiterEnable.prepare (fs, latencyFadeMs);
        stereoMorph.prepare (fs, stereoMorphMs);
        for (auto& p : processAmount)
            p.prepare (fs, stereoMorphMs);
        mbWeight.prepare (fs, multibandFadeMs);
        tubeAmount.prepare (fs, amountSmoothingMs);
        ironAmount.prepare (fs, amountSmoothingMs);

        fadeLength = std::max (64, static_cast<int> (0.01 * fs));

        configureStages (stagesForQuality (params.quality));

        snapToParams();
        reset();
    }

    void HeatEngine::configureStages (int stages) noexcept
    {
        activeStages = stages;
        oversampler.setNumStages (activeStages);
        const double osRate = fs * (1 << activeStages);
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            tube[ch].prepare (osRate);
            iron[ch].prepare (osRate);
            iron[ch].setModelImmediate (params.ironModel);
            colour[ch].prepare (osRate);
        }
    }

    bool HeatEngine::wantsColour() const noexcept
    {
        return params.tube > 0.0f || params.iron > 0.0f
               || tubeAmount.getCurrent() > 0.0f || ironAmount.getCurrent() > 0.0f
               || params.mode != Mode::clean
               || colourDrive[static_cast<size_t> (chunkSize - 1)] > 1.0e-4f;
    }

    void HeatEngine::snapToParams() noexcept
    {
        inputGainDb.reset (params.inputDb);
        outputGainDb.reset (params.outputDb);
        mix.reset (clamp01 (params.mix));
        scExternal.reset (0.0f);
        listen.reset (params.scListen ? 1.0f : 0.0f);
        bypass.reset (params.bypass ? 1.0f : 0.0f);
        limiterEnable.reset (params.limiter ? 1.0f : 0.0f);
        tubeAmount.reset (clamp01 (params.tube));
        ironAmount.reset (clamp01 (params.iron));
        scFilter.setCutoffImmediate (params.hpfHz);
        scFilter.setLowpassImmediate (params.scLpfHz);
        scFilter.setBellImmediate (params.scEqHz, params.scEqDb, params.scEqQ);
        splitter.setCrossoversImmediate (params.xoverLowHz, std::max (params.xoverHighHz, 2.0f * params.xoverLowHz));
        limiter.setCeilingDb (params.ceilingDb);
        compressor.snapToControls (controlsFrom (params));
        setBandControls (chunkSize, true);

        const bool stereo = preparedChannels == 2;
        stereoMorph.reset (stereo ? morphTargetFor (params.stereoMode) : 0.0f);
        processAmount[0].reset (stereo && params.stereoMode == StereoMode::sideOnly ? 0.0f : 1.0f);
        processAmount[1].reset (stereo && params.stereoMode == StereoMode::midOnly ? 0.0f : 1.0f);

        const int la = lookaheadSamplesFor (params.lookaheadMs);
        const int lim = params.limiter ? limiter.getLatency() : 0;
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            lookaheadLine[ch].snapDelay (la);
            listenLine[ch].snapDelay (coreLatency + la);
            limiterLine[ch].snapDelay (lim);
            bypassLine[ch].snapDelay (coreLatency + la + lim);
            iron[ch].setModelImmediate (params.ironModel);
        }
    }

    void HeatEngine::reset() noexcept
    {
        compressor.reset();
        for (auto& b : bandComp)
            b.reset();
        scFilter.reset();
        splitter.reset();
        oversampler.reset();
        limiter.reset();
        limiterRunning = false;
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            for (auto* d : { &bypassLine[ch], &lookaheadLine[ch], &listenLine[ch], &limiterLine[ch] })
                d->reset();
            for (auto* d : { &dryDelay[ch], &gainDelay[ch], &osGainDelay[ch], &osPad[ch], &colourDriveDelay[ch],
                             &colourBiasDelay[ch], &lowRatioDelay[ch], &highRatioDelay[ch], &osLowRatioDelay[ch],
                             &osHighRatioDelay[ch], &processDelay[ch] })
                d->reset();
            tube[ch].reset();
            iron[ch].reset();
            colour[ch].reset();
            for (auto* s : { &baseLow[ch], &baseHigh[ch], &osLow[ch], &osHigh[ch] })
                s->reset();
            osLowPrev[ch] = osHighPrev[ch] = {};
            lastGainLin[ch] = 1.0f;
        }
        morphDelay.reset();

        // The processing-domain values at the output must match those at the
        // input after a reset (the delay lines are silent anyway).
        {
            const float t = stereoMorph.getCurrent();
            for (int i = 0; i <= coreLatency; ++i)
                morphDelay.process (t);
            for (int ch = 0; ch < maxChannels; ++ch)
            {
                const float p = processAmount[ch].getCurrent();
                for (int i = 0; i <= coreLatency; ++i)
                    processDelay[ch].process (p);
            }
        }

        colourState = ColourState::off;
        colourCounter = 0;
        telemetry = {};

        // Multiband starts engaged (or off) at a reset: nothing to fade from.
        mbState = MbState::off;
        mbStructure = MultibandMode::off;
        mbWeight.reset (0.0f);
        if (params.multiband != MultibandMode::off)
        {
            startMultiband();
            mbState = MbState::on;
            mbWeight.reset (1.0f);
        }

        // From a reset every delay line is silent, so the colour path can start
        // fully engaged: both paths output the same silence, no fade needed.
        std::fill (colourDrive.begin(), colourDrive.end(), 0.0f);
        if (wantsColour())
        {
            const int requested = stagesForQuality (params.quality);
            if (requested != activeStages)
                configureStages (requested);
            startColourPath();
            colourState = ColourState::on;
        }
        telemetry.oversamplingFactor = 1 << activeStages;
        telemetry.latencySamples = getLatencySamples();
    }

    void HeatEngine::setBandControls (int blockSize, bool snap) noexcept
    {
        const auto base = controlsFrom (params);
        for (int b = 0; b < 3; ++b)
        {
            auto c = base;
            c.compress = clamp01 (params.compress * std::clamp (params.bandAmount[b], 0.0f, 2.0f));
            c.attackMs = std::clamp (base.attackMs * bandAttackScale[b], attackMinMs, attackMaxMs);
            c.releaseMs = std::clamp (base.releaseMs * bandReleaseScale[b], releaseMinMs, releaseMaxMs);
            if (snap)
                bandComp[b].snapToControls (c);
            else
                bandComp[b].setControls (c, blockSize);
        }
    }

    void HeatEngine::startMultiband() noexcept
    {
        mbStructure = params.multiband;
        setBandControls (chunkSize, true);
        for (auto& b : bandComp)
            b.reset();
        splitter.reset();
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            for (auto* s : { &baseLow[ch], &baseHigh[ch], &osLow[ch], &osHigh[ch] })
                s->reset();
            osLowPrev[ch] = osHighPrev[ch] = {};
            for (auto* d : { &lowRatioDelay[ch], &highRatioDelay[ch], &osLowRatioDelay[ch], &osHighRatioDelay[ch] })
                d->reset();
        }
    }

    void HeatEngine::updateMultibandState (int n) noexcept
    {
        const auto target = params.multiband;
        switch (mbState)
        {
            case MbState::off:
                if (target != MultibandMode::off)
                {
                    startMultiband();
                    mbState = MbState::priming;
                    mbCounter = static_cast<int> (multibandPrimeMs * 1.0e-3 * fs);
                }
                break;

            case MbState::priming:
                mbCounter -= n;
                if (target != mbStructure)
                    mbState = MbState::fadingOut;
                else if (mbCounter <= 0)
                    mbState = MbState::fadingIn;
                break;

            case MbState::fadingIn:
            case MbState::on:
                if (target != mbStructure)
                    mbState = MbState::fadingOut;
                break;

            case MbState::fadingOut:
                // Once the weight is 0, wait until the delayed shelf ratios have
                // drained (they trail the input by up to the core latency)
                // before the shelves are switched off.
                if (! mbWeight.isSmoothing() && mbWeight.getCurrent() <= 0.0f)
                {
                    mbCounter -= n;
                    if (mbCounter <= 0)
                    {
                        mbState = MbState::off;
                        mbStructure = MultibandMode::off;
                    }
                }
                else
                {
                    mbCounter = coreLatency + chunkSize;
                }
                break;
        }
        mbWeight.setTarget (mbState == MbState::fadingIn || mbState == MbState::on ? 1.0f : 0.0f);
    }

    void HeatEngine::process (float* const* io, int numChannels,
                              const float* const* sidechain, int numSidechainChannels,
                              int numSamples) noexcept
    {
        ScopedFlushDenormals ftz;

        const int numCh = std::clamp (numChannels, 1, maxChannels);
        const int numSc = sidechain != nullptr ? std::clamp (numSidechainChannels, 0, maxChannels) : 0;
        const bool stereo = numCh == 2;

        inputGainDb.setTarget (std::clamp (params.inputDb, -24.0f, 24.0f));
        outputGainDb.setTarget (std::clamp (params.outputDb, -24.0f, 24.0f));
        mix.setTarget (clamp01 (params.mix));
        scExternal.setTarget (params.scSource == SidechainSource::external && numSc > 0 ? 1.0f : 0.0f);
        listen.setTarget (params.scListen ? 1.0f : 0.0f);
        bypass.setTarget (params.bypass ? 1.0f : 0.0f);
        limiterEnable.setTarget (params.limiter ? 1.0f : 0.0f);
        tubeAmount.setTarget (clamp01 (params.tube));
        ironAmount.setTarget (clamp01 (params.iron));
        scFilter.setCutoff (std::clamp (params.hpfHz, hpfMinHz, hpfMaxHz));
        scFilter.setLowpass (std::clamp (params.scLpfHz, 1000.0f, SidechainFilter::lpfOffHz));
        scFilter.setBell (std::clamp (params.scEqHz, 20.0f, 20000.0f), std::clamp (params.scEqDb, -18.0f, 18.0f),
                          std::clamp (params.scEqQ, 0.1f, 10.0f));
        splitter.setCrossovers (params.xoverLowHz, std::max (params.xoverHighHz, 2.0f * params.xoverLowHz));
        limiter.setCeilingDb (params.ceilingDb);
        compressor.setControls (controlsFrom (params), numSamples);
        if (mbState != MbState::off || params.multiband != MultibandMode::off)
            setBandControls (numSamples, false);

        stereoMorph.setTarget (stereo ? morphTargetFor (params.stereoMode) : 0.0f);
        processAmount[0].setTarget (stereo && params.stereoMode == StereoMode::sideOnly ? 0.0f : 1.0f);
        processAmount[1].setTarget (stereo && params.stereoMode == StereoMode::midOnly ? 0.0f : 1.0f);

        for (int ch = 0; ch < numCh; ++ch)
            iron[ch].setModel (params.ironModel);

        // Latency-dependent paths (cross-faded when LOOKAHEAD / LIMITER change).
        const int la = lookaheadSamplesFor (params.lookaheadMs);
        const int lim = params.limiter ? limiter.getLatency() : 0;
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            lookaheadLine[ch].setDelay (la);
            listenLine[ch].setDelay (coreLatency + la);
            limiterLine[ch].setDelay (lim);
            bypassLine[ch].setDelay (coreLatency + la + lim);
        }

        telemetry.grDb[0] = telemetry.grDb[1] = 0.0f;
        telemetry.inputPeak = telemetry.outputPeak = 0.0f;
        telemetry.limiterGrDb = 0.0f;
        telemetry.bandGrDb[0] = telemetry.bandGrDb[1] = telemetry.bandGrDb[2] = 0.0f;
        telemetry.recoveredFromNonFinite = false;

        for (int offset = 0; offset < numSamples; offset += chunkSize)
            processChunk (io, numCh, sidechain, numSc, offset, std::min (chunkSize, numSamples - offset));

        if (numCh == 1)
        {
            telemetry.grDb[1] = telemetry.grDb[0];
            telemetry.grDbLast[1] = telemetry.grDbLast[0];
        }

        telemetry.thresholdDb = compressor.getEffectiveThresholdDb();
        telemetry.ratio = compressor.getEffectiveRatio();
        telemetry.kneeDb = compressor.getEffectiveKneeDb();
        telemetry.attackMs = compressor.getBallisticsSettings().attackMs;
        telemetry.releaseMs = compressor.getBallisticsSettings().releaseMs;
        telemetry.colourPathActive = colourState != ColourState::off;
        telemetry.oversamplingFactor = 1 << activeStages;
        telemetry.multibandActive = mbState != MbState::off;
        telemetry.midSideActive = stereoMorph.getCurrent() > 0.0f;
        telemetry.latencySamples = getLatencySamples();

        if (telemetry.recoveredFromNonFinite)
            reset();
    }

    void HeatEngine::startColourPath() noexcept
    {
        oversampler.reset();
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            osGainDelay[ch].reset();
            osGainDelay[ch].setDelay (gainDelayPerStages[activeStages]);
            osPad[ch].reset();
            osPad[ch].setDelay (padPerStages[activeStages]);
            colourDriveDelay[ch].reset();
            colourDriveDelay[ch].setDelay (driveDelayPerStages[activeStages]);
            colourBiasDelay[ch].reset();
            colourBiasDelay[ch].setDelay (driveDelayPerStages[activeStages]);
            osLowRatioDelay[ch].reset();
            osLowRatioDelay[ch].setDelay (driveDelayPerStages[activeStages]);
            osHighRatioDelay[ch].reset();
            osHighRatioDelay[ch].setDelay (driveDelayPerStages[activeStages]);
            osLow[ch].reset();
            osHigh[ch].reset();
            osLowPrev[ch] = osHighPrev[ch] = {};
            tube[ch].reset();
            iron[ch].reset();
            colour[ch].reset();
        }
        colourState = ColourState::priming;
        colourCounter = 2 * coreLatency + 64;
    }

    void HeatEngine::updateColourState (int n) noexcept
    {
        const bool wantColour = wantsColour();
        const int requestedStages = stagesForQuality (params.quality);

        switch (colourState)
        {
            case ColourState::off:
                if (wantColour)
                {
                    if (requestedStages != activeStages)
                        configureStages (requestedStages);
                    startColourPath();
                }
                break;

            case ColourState::priming:
                colourCounter -= n;
                if (colourCounter <= 0)
                {
                    colourState = ColourState::fadingIn;
                    colourCounter = 0;
                }
                break;

            case ColourState::on:
                if (! wantColour || requestedStages != activeStages)
                {
                    colourState = ColourState::fadingOut;
                    colourCounter = 0;
                }
                break;

            case ColourState::fadingIn:
            case ColourState::fadingOut:
                break;
        }
    }

    void HeatEngine::runColourPath (int numCh, int n) noexcept
    {
        const int factor = 1 << activeStages;
        const float invFactor = 1.0f / static_cast<float> (factor);
        const bool tubeOn = tubeA[static_cast<size_t> (0)] > 0.0f || tubeA[static_cast<size_t> (n - 1)] > 0.0f;
        const bool ironOn = ironA[static_cast<size_t> (0)] > 0.0f || ironA[static_cast<size_t> (n - 1)] > 0.0f;
        const bool shelves = mbInChunk;
        const bool highShelf = mbStructure == MultibandMode::threeBand;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* hi = oversampler.upsample (ch, audP[ch].data(), n);
            float gPrev = lastGainLin[ch];

            for (int i = 0; i < n; ++i)
            {
                const auto idx = static_cast<size_t> (i);
                if (tubeOn)
                    tube[ch].setAmount (tubeA[idx]);
                if (ironOn)
                    iron[ch].setAmount (ironA[idx]);
                colour[ch].set (colourDriveDelay[ch].process (drive[ch][idx]),
                                colourBiasDelay[ch].process (colourBias[idx]));

                ShelfCoefficients lowC, highC;
                if (shelves)
                {
                    lowC = ShelfCoefficients::lowShelf (shelfG0Os[0], osLowRatioDelay[ch].process (lowRatio[ch][idx]));
                    highC = ShelfCoefficients::highShelf (shelfG0Os[1], osHighRatioDelay[ch].process (highRatio[ch][idx]));
                }

                const float gCur = gainLin[ch][idx];
                float* block = hi + i * factor;
                for (int k = 0; k < factor; ++k)
                {
                    float v = block[k];
                    if (tubeOn)
                        v = tube[ch].process (v);
                    const float t = static_cast<float> (k + 1) * invFactor;
                    const float gi = gPrev + t * (gCur - gPrev);
                    v *= osGainDelay[ch].process (gi);
                    if (shelves)
                    {
                        v = osLow[ch].process (v, ShelfCoefficients::lerp (osLowPrev[ch], lowC, t));
                        if (highShelf)
                            v = osHigh[ch].process (v, ShelfCoefficients::lerp (osHighPrev[ch], highC, t));
                    }
                    v = colour[ch].process (v);
                    if (ironOn)
                        v = iron[ch].process (v);
                    block[k] = osPad[ch].process (v);
                }
                gPrev = gCur;
                if (shelves)
                {
                    osLowPrev[ch] = lowC;
                    osHighPrev[ch] = highC;
                }
            }
            lastGainLin[ch] = gPrev;
            oversampler.downsample (ch, wetOs[ch].data(), n);
        }
    }

    void HeatEngine::computeGains (int numCh, int n) noexcept
    {
        // Full-band gain (and, with MULTIBAND, the band gains cross-faded in
        // from it) → reference gain + shelf ratios per channel.
        const bool three = mbStructure == MultibandMode::threeBand;
        for (int i = 0; i < n; ++i)
            mbW[static_cast<size_t> (i)] = mbInChunk ? smoothstep (mbWeight.next()) : 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float deepest = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const auto idx = static_cast<size_t> (i);
                const float grFull = gr[ch][idx];
                float grShown = grFull;

                if (mbInChunk)
                {
                    const float w = mbW[idx];
                    const float full = grFull + makeup[idx];
                    auto bandDb = [&] (int b) { return full + w * (bandGr[b][ch][idx] + bandMakeup[b][idx] - full); };
                    auto bandGrDb = [&] (int b) { return grFull + w * (bandGr[b][ch][idx] - grFull); };

                    const float lowDb = bandDb (0);
                    const float highDb = bandDb (2);
                    const float refDb = three ? bandDb (1) : highDb;
                    gainLin[ch][idx] = dbToGain (refDb);
                    lowRatio[ch][idx] = lowDb - refDb;
                    highRatio[ch][idx] = three ? highDb - refDb : 0.0f;
                    grShown = std::min (bandGrDb (0), bandGrDb (2));
                    if (three)
                        grShown = std::min (grShown, bandGrDb (1));
                }
                else
                {
                    gainLin[ch][idx] = dbToGain (grFull) * dbToGain (makeup[idx]);
                }

                grEff[ch][idx] = grShown;
                gainDelayed[ch][idx] = gainDelay[ch].process (gainLin[ch][idx]);
                drive[ch][idx] = colourDrive[idx] + colourInteraction[idx] * (1.0f - dbToGain (grShown));
                deepest = std::min (deepest, grShown * pIn[ch][idx]);
            }

            if (mbInChunk)
                for (int i = 0; i < n; ++i)
                {
                    const auto idx = static_cast<size_t> (i);
                    lowRatioD[ch][idx] = lowRatioDelay[ch].process (lowRatio[ch][idx]);
                    highRatioD[ch][idx] = highRatioDelay[ch].process (highRatio[ch][idx]);
                }

            telemetry.grDb[ch] = std::min (telemetry.grDb[ch], deepest);
            telemetry.grDbLast[ch] = grEff[ch][static_cast<size_t> (n - 1)] * pIn[ch][static_cast<size_t> (n - 1)];
        }

        if (mbInChunk)
            for (int b = 0; b < 3; ++b)
                for (int ch = 0; ch < numCh; ++ch)
                    for (int i = 0; i < n; ++i)
                        telemetry.bandGrDb[b] = std::min (telemetry.bandGrDb[b], bandGr[b][ch][static_cast<size_t> (i)]);
    }

    void HeatEngine::processChunk (float* const* io, int numCh, const float* const* sidechain, int numSc,
                                   int offset, int n) noexcept
    {
        const bool stereo = numCh == 2;

        // --- Input gain, bypass copy, look-ahead ---------------------------------------
        for (int i = 0; i < n; ++i)
        {
            const float gIn = dbToGain (inputGainDb.next());
            for (int ch = 0; ch < numCh; ++ch)
            {
                const auto idx = static_cast<size_t> (i);
                const float raw = io[ch][offset + i];
                bypassed[ch][idx] = bypassLine[ch].process (raw);
                const float x = raw * gIn;
                pre[ch][idx] = x;
                aud[ch][idx] = lookaheadLine[ch].process (x);
                telemetry.inputPeak = std::max (telemetry.inputPeak, std::abs (raw));
            }
        }

        // --- Dry path (latency aligned) -----------------------------------------------
        for (int ch = 0; ch < numCh; ++ch)
            dryDelay[ch].process (aud[ch].data(), dry[ch].data(), n);

        // --- Sidechain: source cross-fade, HPF / LPF / BELL ------------------------------
        for (int i = 0; i < n; ++i)
        {
            const float w = scExternal.next();
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float internal = pre[ch][static_cast<size_t> (i)];
                const float external = numSc > 0 ? sidechain[std::min (ch, numSc - 1)][offset + i] : internal;
                sc[ch][static_cast<size_t> (i)] = internal + w * (external - internal);
            }
        }
        float* scPtrs[maxChannels] = { sc[0].data(), sc[1].data() };
        scFilter.process (scPtrs, numCh, n);

        // --- SC listen (L/R, aligned with the audio) ------------------------------------
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < n; ++i)
                listenSig[ch][static_cast<size_t> (i)] = listenLine[ch].process (sc[ch][static_cast<size_t> (i)]);

        // --- Stereo processing domain ----------------------------------------------------
        msInChunk = msOutChunk = false;
        for (int i = 0; i < n; ++i)
        {
            const auto idx = static_cast<size_t> (i);
            const float t = smoothstep (stereoMorph.next());
            tIn[idx] = t;
            tOut[idx] = morphDelay.process (t);
            msInChunk = msInChunk || t > 0.0f;
            msOutChunk = msOutChunk || tOut[idx] > 0.0f;
            for (int ch = 0; ch < maxChannels; ++ch)
            {
                const float p = smoothstep (processAmount[ch].next());
                pIn[ch][idx] = p;
                pOut[ch][idx] = processDelay[ch].process (p);
            }
        }
        msInChunk = msInChunk && stereo;
        msOutChunk = msOutChunk && stereo;

        if (msInChunk)
        {
            for (int i = 0; i < n; ++i)
            {
                const auto idx = static_cast<size_t> (i);
                const auto m = StereoMatrix::forMorph (tIn[idx]);
                m.encode (sc[0][idx], sc[1][idx], scP[0][idx], scP[1][idx]);
                m.encode (aud[0][idx], aud[1][idx], audP[0][idx], audP[1][idx]);
            }
        }
        else
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                std::copy (sc[ch].begin(), sc[ch].begin() + n, scP[ch].begin());
                std::copy (aud[ch].begin(), aud[ch].begin() + n, audP[ch].begin());
            }
        }

        // --- Compressor --------------------------------------------------------------
        CompressorEngine::Outputs out;
        out.grDb[0] = gr[0].data();
        out.grDb[1] = gr[1].data();
        out.makeupDb = makeup.data();
        out.colorDrive = colourDrive.data();
        out.colorBias = colourBias.data();
        out.colorInteraction = colourInteraction.data();
        const float* scConst[maxChannels] = { scP[0].data(), scP[1].data() };
        compressor.process (scConst, numCh, n, out);

        // --- Multiband detection ------------------------------------------------------
        updateMultibandState (n);
        mbInChunk = mbState != MbState::off;
        if (mbInChunk)
        {
            const bool three = mbStructure == MultibandMode::threeBand;
            float* const bandPtrs[3][maxChannels] = { { bandSc[0][0].data(), bandSc[0][1].data() },
                                                      { bandSc[1][0].data(), bandSc[1][1].data() },
                                                      { bandSc[2][0].data(), bandSc[2][1].data() } };
            splitter.process (scConst, numCh, n, three, bandPtrs);
            for (int b = 0; b < 3; ++b)
            {
                if (b == 1 && ! three)
                    continue;
                CompressorEngine::Outputs bo;
                bo.grDb[0] = bandGr[b][0].data();
                bo.grDb[1] = bandGr[b][1].data();
                bo.makeupDb = bandMakeup[b].data();
                const float* bandIn[maxChannels] = { bandSc[b][0].data(), bandSc[b][1].data() };
                bandComp[b].process (bandIn, numCh, n, bo);
            }
            if (! three)
                for (int ch = 0; ch < numCh; ++ch)
                    std::fill (bandGr[1][ch].begin(), bandGr[1][ch].begin() + n, 0.0f);

            const double lo = splitter.getLowHz(), hiHz = splitter.getHighHz();
            shelfG0Base[0] = static_cast<float> (std::tan (pi * std::min (lo, 0.45 * fs) / fs));
            shelfG0Base[1] = static_cast<float> (std::tan (pi * std::min (hiHz, 0.45 * fs) / fs));
            const double osRate = fs * (1 << activeStages);
            shelfG0Os[0] = static_cast<float> (std::tan (pi * std::min (lo, 0.45 * osRate) / osRate));
            shelfG0Os[1] = static_cast<float> (std::tan (pi * std::min (hiHz, 0.45 * osRate) / osRate));
        }

        computeGains (numCh, n);
        telemetry.makeupDb = makeup[static_cast<size_t> (n - 1)];

        // --- Colour amounts (smoothed per base sample) -------------------------------
        for (int i = 0; i < n; ++i)
        {
            tubeA[static_cast<size_t> (i)] = tubeAmount.next();
            ironA[static_cast<size_t> (i)] = ironAmount.next();
        }
        if (n < chunkSize)
            colourDrive[static_cast<size_t> (chunkSize - 1)] = colourDrive[static_cast<size_t> (n - 1)];

        // --- Wet path ------------------------------------------------------------------
        updateColourState (n);
        const bool osRunning = colourState != ColourState::off;
        if (osRunning)
            runColourPath (numCh, n);

        // --- Combine -------------------------------------------------------------------
        const bool highShelf = mbStructure == MultibandMode::threeBand;
        const bool limiterNeeded = params.limiter || limiterEnable.isSmoothing() || limiterEnable.getCurrent() > 0.0f;
        if (limiterNeeded && ! limiterRunning)
            limiter.reset(); // start detection from a clean state (the enable fade covers its first window)
        limiterRunning = limiterNeeded;
        bool nonFinite = false;
        float limiterMin = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto idx = static_cast<size_t> (i);

            float wOs = 0.0f;
            switch (colourState)
            {
                case ColourState::off:
                case ColourState::priming:  wOs = 0.0f; break;
                case ColourState::on:       wOs = 1.0f; break;
                case ColourState::fadingIn:
                    wOs = std::min (1.0f, static_cast<float> (colourCounter) / static_cast<float> (fadeLength));
                    if (++colourCounter >= fadeLength)
                        colourState = ColourState::on;
                    break;
                case ColourState::fadingOut:
                    wOs = std::max (0.0f, 1.0f - static_cast<float> (colourCounter) / static_cast<float> (fadeLength));
                    if (++colourCounter >= fadeLength)
                        colourState = ColourState::off;
                    break;
            }

            const float m = mix.next();
            const float wListen = listen.next();
            const float gOut = dbToGain (outputGainDb.next());
            const float wBypass = bypass.next();
            const float wLimiter = smoothstep (limiterEnable.next());

            // Wet in the processing domain.
            const auto matrix = msOutChunk ? StereoMatrix::forMorph (tOut[idx]) : StereoMatrix {};
            float neutral[maxChannels] { dry[0][idx], numCh > 1 ? dry[1][idx] : 0.0f };
            if (msOutChunk)
                matrix.encode (dry[0][idx], dry[1][idx], neutral[0], neutral[1]);

            float wet[maxChannels] { 0.0f, 0.0f };
            for (int ch = 0; ch < numCh; ++ch)
            {
                float wetDelay = neutral[ch] * gainDelayed[ch][idx];
                if (mbInChunk)
                {
                    wetDelay = baseLow[ch].process (wetDelay, ShelfCoefficients::lowShelf (shelfG0Base[0], lowRatioD[ch][idx]));
                    if (highShelf)
                        wetDelay = baseHigh[ch].process (wetDelay, ShelfCoefficients::highShelf (shelfG0Base[1], highRatioD[ch][idx]));
                }
                float w = osRunning ? wetDelay + wOs * (wetOs[ch][idx] - wetDelay) : wetDelay;
                const float p = pOut[ch][idx];
                if (p < 1.0f)
                    w = neutral[ch] + p * (w - neutral[ch]); // MID / SIDE only: unprocessed channel
                wet[ch] = w;
            }
            if (msOutChunk)
                matrix.decode (wet[0], wet[1], wet[0], wet[1]);

            float frame[maxChannels] { 0.0f, 0.0f };
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float d = dry[ch][idx];
                float y = d + m * (wet[ch] - d);
                y += wListen * (listenSig[ch][idx] - y);
                frame[ch] = y * gOut;
            }

            // Output limiter (linked); not computed at all while off.
            const float gLim = limiterRunning ? limiter.process (frame, numCh) : 1.0f;
            const float gApplied = 1.0f + wLimiter * (gLim - 1.0f);
            if (wLimiter > 0.0f)
                limiterMin = std::min (limiterMin, gApplied);

            for (int ch = 0; ch < numCh; ++ch)
            {
                float y = limiterLine[ch].process (frame[ch]) * gApplied;
                y += wBypass * (bypassed[ch][idx] - y);

                if (! isFiniteSample (y))
                {
                    nonFinite = true;
                    y = 0.0f;
                }
                y = std::clamp (y, -safetyCeiling, safetyCeiling);
                io[ch][offset + i] = y;
                telemetry.outputPeak = std::max (telemetry.outputPeak, std::abs (y));
            }
        }

        telemetry.limiterGrDb = std::min (telemetry.limiterGrDb, gainToDb (limiterMin));
        if (nonFinite)
            telemetry.recoveredFromNonFinite = true;
    }
}
