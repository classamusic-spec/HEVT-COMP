#include "DSP/CompressorEngine.h"
#include "DSP/GainComputer.h"

namespace heat::dsp
{
    namespace
    {
        constexpr float curveSmoothingMs = 40.0f;
        constexpr float detectorFadeMs = 25.0f;
        constexpr float linkFadeMs = 25.0f;
        constexpr float timingSmoothingMs = 50.0f;

        float linkAmountFor (StereoLink link) noexcept
        {
            switch (link)
            {
                case StereoLink::partial:  return 0.5f;
                case StereoLink::dualMono: return 0.0f;
                case StereoLink::linked:
                default:                   return 1.0f;
            }
        }

        // Blends ballistics settings towards a target (block-rate smoothing).
        void glide (BallisticsSettings& s, const BallisticsSettings& t, float a) noexcept
        {
            // Times glide in the log domain so a 0.1 ms → 100 ms change is even.
            auto logGlide = [a] (float& v, float target)
            {
                const float lv = std::log (std::max (1.0e-4f, v));
                const float lt = std::log (std::max (1.0e-4f, target));
                v = std::exp (lt + a * (lv - lt));
            };
            auto linGlide = [a] (float& v, float target) { v = target + a * (v - target); };

            logGlide (s.attackMs, t.attackMs);
            logGlide (s.releaseMs, t.releaseMs);
            linGlide (s.roundingMs, t.roundingMs);
            linGlide (s.memoryWeight, t.memoryWeight);
            linGlide (s.memoryDepth, t.memoryDepth);
            logGlide (s.memoryChargeMs, t.memoryChargeMs);
            logGlide (s.memoryReleaseMs, t.memoryReleaseMs);
            linGlide (s.attackBoostPerDb, t.attackBoostPerDb);
        }
    }

    void CompressorEngine::prepare (double sampleRate) noexcept
    {
        fs = sampleRate;

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            rms[ch].prepare (sampleRate);
            optical[ch].prepare (sampleRate);
            ballistics[ch].prepare (sampleRate);
        }

        for (auto* s : { &thr, &slope, &knee, &makeupFactor, &colorDrive, &colorGrInteraction, &colorBias, &softening })
            s->prepare (sampleRate, curveSmoothingMs);

        for (auto& w : detWeight)
            w.prepare (sampleRate, detectorFadeMs);

        linkAmount.prepare (sampleRate, linkFadeMs);
        for (auto& w : linkInclude)
            w.prepare (sampleRate, linkFadeMs);

        snapToControls (controls);
        reset();
    }

    void CompressorEngine::reset() noexcept
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            peak[ch].reset();
            rms[ch].reset();
            optical[ch].reset();
            ballistics[ch].reset (0.0f);
        }
    }

    float CompressorEngine::getEffectiveRatio() const noexcept
    {
        const float s = slope.getCurrent();
        return s > -0.9999f ? 1.0f / (1.0f + s) : 10000.0f;
    }

    void CompressorEngine::applyTargets (const Controls& c) noexcept
    {
        controls = c;

        const auto macro = CompressMacro::evaluate (c.compress, c.mode);
        const auto& profile = getModeProfile (c.mode);

        thr.setTarget (macro.thresholdDb);
        slope.setTarget (GainComputer::slopeForRatio (macro.ratio));
        knee.setTarget (macro.kneeDb);
        makeupFactor.setTarget (c.autoMakeup ? macro.makeupFactor : 0.0f);
        colorDrive.setTarget (macro.colorDrive);
        colorGrInteraction.setTarget (profile.colorGrInteraction);
        colorBias.setTarget (profile.colorBias);
        softening.setTarget (profile.detectorSoftening);

        for (int d = 0; d < numDetectors; ++d)
            detWeight[d].setTarget (static_cast<int> (c.detector) == d ? 1.0f : 0.0f);

        linkAmount.setTarget (linkAmountFor (c.link));
        for (int ch = 0; ch < maxChannels; ++ch)
            linkInclude[ch].setTarget (c.linkInclude[ch] ? 1.0f : 0.0f);

        timingTarget = CompressMacro::ballistics (c.mode, c.detector, c.attackMs, c.releaseMs, c.autoRelease);
    }

    void CompressorEngine::setControls (const Controls& c, int blockSize) noexcept
    {
        applyTargets (c);

        const float a = std::exp (-static_cast<float> (blockSize) / (timingSmoothingMs * 1.0e-3f * static_cast<float> (fs)));
        glide (timing, timingTarget, a);
        for (auto& b : ballistics)
            b.setSettings (timing);
    }

    void CompressorEngine::snapToControls (const Controls& c) noexcept
    {
        applyTargets (c);

        for (auto* s : { &thr, &slope, &knee, &makeupFactor, &colorDrive, &colorGrInteraction, &colorBias, &softening })
            s->reset (s->getTarget());
        for (auto& w : detWeight)
            w.reset (w.getTarget());
        linkAmount.reset (linkAmount.getTarget());
        for (auto& w : linkInclude)
            w.reset (w.getTarget());

        timing = timingTarget;
        for (auto& b : ballistics)
            b.setSettings (timing);
    }

    void CompressorEngine::process (const float* const* sidechain, int numChannels, int numSamples, const Outputs& out) noexcept
    {
        numChannels = std::clamp (numChannels, 1, maxChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            const float wPeak = detWeight[0].next();
            const float wRms = detWeight[1].next();
            const float wOpt = detWeight[2].next();
            const float soft = softening.next();
            const float link = linkAmount.next();

            const float t = thr.next();
            const float s = slope.next();
            const float k = knee.next();
            const float mf = makeupFactor.next();

            float level[maxChannels];
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x = sidechain[ch][i];
                const float lp = peak[ch].processDb (x);
                const float lr = rms[ch].processDb (x);
                const float lo = optical[ch].processDb (x);
                const float lpSoft = lp + soft * (lr - lp);
                level[ch] = wPeak * lpSoft + wRms * lr + wOpt * lo;
            }

            if (numChannels == 2)
            {
                // An excluded channel is pushed far below the other before the max.
                constexpr float excludedDb = -200.0f;
                const float i0 = linkInclude[0].next(), i1 = linkInclude[1].next();
                const float linked = std::max (level[0] + (1.0f - i0) * excludedDb,
                                               level[1] + (1.0f - i1) * excludedDb);
                level[0] += link * (linked - level[0]);
                level[1] += link * (linked - level[1]);
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float target = GainComputer::gainReductionDb (level[ch], t, s, k);
                out.grDb[ch][i] = ballistics[ch].process (target);
            }

            if (out.makeupDb != nullptr)
            {
                const float grRef = GainComputer::gainReductionDb (CompressMacro::makeupReferenceDb, t, s, k);
                out.makeupDb[i] = std::clamp (-grRef * mf, 0.0f, CompressMacro::makeupLimitDb);
            }

            const float cd = colorDrive.next();
            const float cb = colorBias.next();
            const float ci = colorGrInteraction.next();
            if (out.colorDrive != nullptr)
                out.colorDrive[i] = cd;
            if (out.colorBias != nullptr)
                out.colorBias[i] = cb;
            if (out.colorInteraction != nullptr)
                out.colorInteraction[i] = ci;
        }
    }
}
