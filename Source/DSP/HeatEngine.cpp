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
        constexpr float safetyCeiling = 31.6f; // ≈ +30 dBFS: never reached by sane material

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
    }

    int HeatEngine::stagesForQuality (Quality q) const noexcept
    {
        return stagesPerQuality[std::clamp (static_cast<int> (q), 0, 2)];
    }

    void HeatEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        (void) maxBlockSize; // the engine processes in fixed chunks of chunkSize
        fs = sampleRate;
        preparedChannels = std::clamp (numChannels, 1, maxChannels);

        compressor.prepare (fs);
        scFilter.prepare (fs);
        oversampler.prepare (chunkSize);

        // Above 100 kHz the host rate already provides headroom for harmonics:
        // drop one 2x stage (two above 150 kHz) to keep CPU sane.
        const int reduction = fs > 150000.0 ? 2 : fs > 100000.0 ? 1 : 0;
        for (int q = 0; q < 3; ++q)
            stagesPerQuality[q] = std::max (1, q + 1 - reduction);

        const int maxStagesUsed = stagesPerQuality[2];
        latency = static_cast<int> (std::ceil (oversampler.getRoundTripLatencyBaseRate (maxStagesUsed) - 1.0e-9));

        int maxPad = 0, maxGainDelay = 0, maxDriveDelay = 0;
        for (int s = 1; s <= Oversampler::maxStages; ++s)
        {
            const int factor = 1 << s;
            const int roundTripHigh = static_cast<int> (std::lround (oversampler.getRoundTripLatencyBaseRate (s) * factor));
            padPerStages[s] = std::max (0, latency * factor - roundTripHigh);
            const int upLatency = filterLatencyAtTopRate (s);
            gainDelayPerStages[s] = std::max (0, upLatency - (factor - 1));
            driveDelayPerStages[s] = (upLatency + factor / 2) / factor;
            maxPad = std::max (maxPad, padPerStages[s]);
            maxGainDelay = std::max (maxGainDelay, gainDelayPerStages[s]);
            maxDriveDelay = std::max (maxDriveDelay, driveDelayPerStages[s]);
        }

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            for (auto* d : { &bypassDelay[ch], &dryDelay[ch], &gainDelay[ch], &listenDelay[ch] })
            {
                d->prepare (latency);
                d->setDelay (latency);
            }
            osGainDelay[ch].prepare (maxGainDelay);
            osPad[ch].prepare (maxPad);
            colourDriveDelay[ch].prepare (maxDriveDelay);
            colourBiasDelay[ch].prepare (maxDriveDelay);

            for (auto* v : { &pre[ch], &dry[ch], &sc[ch], &gr[ch], &gainLin[ch], &gainDelayed[ch],
                             &wetOs[ch], &bypassed[ch], &listenSig[ch], &drive[ch] })
                v->assign (chunkSize, 0.0f);
        }
        for (auto* v : { &makeup, &colourDrive, &colourBias, &colourInteraction, &tubeA, &ironA })
            v->assign (chunkSize, 0.0f);

        inputGainDb.prepare (fs, gainSmoothingMs);
        outputGainDb.prepare (fs, gainSmoothingMs);
        mix.prepare (fs, mixSmoothingMs);
        scExternal.prepare (fs, sourceFadeMs);
        listen.prepare (fs, listenFadeMs);
        bypass.prepare (fs, bypassFadeMs);
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
        tubeAmount.reset (clamp01 (params.tube));
        ironAmount.reset (clamp01 (params.iron));
        scFilter.setCutoffImmediate (params.hpfHz);
        compressor.snapToControls (controlsFrom (params));
    }

    void HeatEngine::reset() noexcept
    {
        compressor.reset();
        scFilter.reset();
        oversampler.reset();
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            for (auto* d : { &bypassDelay[ch], &dryDelay[ch], &gainDelay[ch], &listenDelay[ch],
                             &osGainDelay[ch], &osPad[ch], &colourDriveDelay[ch], &colourBiasDelay[ch] })
                d->reset();
            tube[ch].reset();
            iron[ch].reset();
            colour[ch].reset();
            lastGainLin[ch] = 1.0f;
        }
        colourState = ColourState::off;
        colourCounter = 0;
        telemetry = {};

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
    }

    void HeatEngine::process (float* const* io, int numChannels,
                              const float* const* sidechain, int numSidechainChannels,
                              int numSamples) noexcept
    {
        ScopedFlushDenormals ftz;

        const int numCh = std::clamp (numChannels, 1, maxChannels);
        const int numSc = sidechain != nullptr ? std::clamp (numSidechainChannels, 0, maxChannels) : 0;

        inputGainDb.setTarget (std::clamp (params.inputDb, -24.0f, 24.0f));
        outputGainDb.setTarget (std::clamp (params.outputDb, -24.0f, 24.0f));
        mix.setTarget (clamp01 (params.mix));
        scExternal.setTarget (params.scSource == SidechainSource::external && numSc > 0 ? 1.0f : 0.0f);
        listen.setTarget (params.scListen ? 1.0f : 0.0f);
        bypass.setTarget (params.bypass ? 1.0f : 0.0f);
        tubeAmount.setTarget (clamp01 (params.tube));
        ironAmount.setTarget (clamp01 (params.iron));
        scFilter.setCutoff (std::clamp (params.hpfHz, hpfMinHz, hpfMaxHz));
        compressor.setControls (controlsFrom (params), numSamples);

        telemetry.grDb[0] = telemetry.grDb[1] = 0.0f;
        telemetry.inputPeak = telemetry.outputPeak = 0.0f;
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
            tube[ch].reset();
            iron[ch].reset();
            colour[ch].reset();
        }
        colourState = ColourState::priming;
        colourCounter = 2 * latency + 64;
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

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* hi = oversampler.upsample (ch, pre[ch].data(), n);
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

                const float gCur = gainLin[ch][idx];
                float* block = hi + i * factor;
                for (int k = 0; k < factor; ++k)
                {
                    float v = block[k];
                    if (tubeOn)
                        v = tube[ch].process (v);
                    const float gi = gPrev + static_cast<float> (k + 1) * invFactor * (gCur - gPrev);
                    v *= osGainDelay[ch].process (gi);
                    v = colour[ch].process (v);
                    if (ironOn)
                        v = iron[ch].process (v);
                    block[k] = osPad[ch].process (v);
                }
                gPrev = gCur;
            }
            lastGainLin[ch] = gPrev;
            oversampler.downsample (ch, wetOs[ch].data(), n);
        }
    }

    void HeatEngine::processChunk (float* const* io, int numCh, const float* const* sidechain, int numSc,
                                   int offset, int n) noexcept
    {
        // --- Input gain, bypass copy -------------------------------------------------
        for (int i = 0; i < n; ++i)
        {
            const float gIn = dbToGain (inputGainDb.next());
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float raw = io[ch][offset + i];
                bypassed[ch][static_cast<size_t> (i)] = bypassDelay[ch].process (raw);
                pre[ch][static_cast<size_t> (i)] = raw * gIn;
                telemetry.inputPeak = std::max (telemetry.inputPeak, std::abs (raw));
            }
        }

        // --- Dry path (latency aligned) -----------------------------------------------
        for (int ch = 0; ch < numCh; ++ch)
            dryDelay[ch].process (pre[ch].data(), dry[ch].data(), n);

        // --- Sidechain: source cross-fade, HPF -------------------------------------------
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

        // --- Compressor --------------------------------------------------------------
        CompressorEngine::Outputs out;
        out.grDb[0] = gr[0].data();
        out.grDb[1] = gr[1].data();
        out.makeupDb = makeup.data();
        out.colorDrive = colourDrive.data();
        out.colorBias = colourBias.data();
        out.colorInteraction = colourInteraction.data();
        const float* scConst[maxChannels] = { sc[0].data(), sc[1].data() };
        compressor.process (scConst, numCh, n, out);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float deepest = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const auto idx = static_cast<size_t> (i);
                const float grDb = gr[ch][idx];
                deepest = std::min (deepest, grDb);
                const float grLin = dbToGain (grDb);
                gainLin[ch][idx] = grLin * dbToGain (makeup[idx]);
                gainDelayed[ch][idx] = gainDelay[ch].process (gainLin[ch][idx]);
                drive[ch][idx] = colourDrive[idx] + colourInteraction[idx] * (1.0f - grLin);
            }
            telemetry.grDb[ch] = std::min (telemetry.grDb[ch], deepest);
            telemetry.grDbLast[ch] = gr[ch][static_cast<size_t> (n - 1)];
        }
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

        // --- SC listen (aligned) -------------------------------------------------------
        for (int ch = 0; ch < numCh; ++ch)
            listenDelay[ch].process (sc[ch].data(), listenSig[ch].data(), n);

        // --- Combine -------------------------------------------------------------------
        bool nonFinite = false;
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

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float d = dry[ch][idx];
                const float wetDelay = d * gainDelayed[ch][idx];
                const float wet = osRunning ? wetDelay + wOs * (wetOs[ch][idx] - wetDelay) : wetDelay;

                float y = d + m * (wet - d);
                y += wListen * (listenSig[ch][idx] - y);
                y *= gOut;
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

        if (nonFinite)
            telemetry.recoveredFromNonFinite = true;
    }
}
