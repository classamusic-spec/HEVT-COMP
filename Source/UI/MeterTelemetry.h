#pragma once

#include "DSP/HeatEngine.h"

#include <atomic>
#include <cstdint>

namespace heat
{
    // Lock-free audio → UI telemetry. The audio thread publishes once per
    // block; the UI reads at frame rate. Gain reduction is accumulated as the
    // deepest value since the UI last consumed it, so no peak is ever missed
    // regardless of block size or frame rate.
    class MeterTelemetry
    {
    public:
        void publish (const dsp::EngineTelemetry& t) noexcept
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                accumulateMin (deepestGrDb[ch], t.grDb[ch]);
                currentGrDb[ch].store (t.grDbLast[ch], std::memory_order_relaxed);
            }
            thresholdDb.store (t.thresholdDb, std::memory_order_relaxed);
            ratio.store (t.ratio, std::memory_order_relaxed);
            kneeDb.store (t.kneeDb, std::memory_order_relaxed);
            makeupDb.store (t.makeupDb, std::memory_order_relaxed);
            attackMs.store (t.attackMs, std::memory_order_relaxed);
            releaseMs.store (t.releaseMs, std::memory_order_relaxed);
            accumulateMax (inputPeak, t.inputPeak);
            accumulateMax (outputPeak, t.outputPeak);
            oversamplingFactor.store (t.oversamplingFactor, std::memory_order_relaxed);
            colourActive.store (t.colourPathActive, std::memory_order_relaxed);
            accumulateMin (limiterGrDb, t.limiterGrDb);
            for (int b = 0; b < 3; ++b)
                accumulateMin (bandGrDb[b], t.bandGrDb[b]);
            multibandActive.store (t.multibandActive, std::memory_order_relaxed);
            midSideActive.store (t.midSideActive, std::memory_order_relaxed);
            latencySamples.store (t.latencySamples, std::memory_order_relaxed);
            blocks.fetch_add (1, std::memory_order_release);
        }

        // UI thread: deepest reduction since the previous call (dB, <= 0).
        float consumeDeepestGrDb (int channel) noexcept
        {
            return deepestGrDb[channel].exchange (0.0f, std::memory_order_acq_rel);
        }

        float consumeInputPeak() noexcept  { return inputPeak.exchange (0.0f, std::memory_order_acq_rel); }
        float consumeLimiterGrDb() noexcept { return limiterGrDb.exchange (0.0f, std::memory_order_acq_rel); }
        float consumeBandGrDb (int band) noexcept { return bandGrDb[band].exchange (0.0f, std::memory_order_acq_rel); }
        bool isMultibandActive() const noexcept { return multibandActive.load (std::memory_order_relaxed); }
        bool isMidSideActive() const noexcept { return midSideActive.load (std::memory_order_relaxed); }
        int getLatencySamples() const noexcept { return latencySamples.load (std::memory_order_relaxed); }
        float consumeOutputPeak() noexcept { return outputPeak.exchange (0.0f, std::memory_order_acq_rel); }

        float getCurrentGrDb (int channel) const noexcept { return currentGrDb[channel].load (std::memory_order_relaxed); }
        float getThresholdDb() const noexcept { return thresholdDb.load (std::memory_order_relaxed); }
        float getRatio() const noexcept { return ratio.load (std::memory_order_relaxed); }
        float getKneeDb() const noexcept { return kneeDb.load (std::memory_order_relaxed); }
        float getMakeupDb() const noexcept { return makeupDb.load (std::memory_order_relaxed); }
        float getAttackMs() const noexcept { return attackMs.load (std::memory_order_relaxed); }
        float getReleaseMs() const noexcept { return releaseMs.load (std::memory_order_relaxed); }
        int getOversamplingFactor() const noexcept { return oversamplingFactor.load (std::memory_order_relaxed); }
        bool isColourActive() const noexcept { return colourActive.load (std::memory_order_relaxed); }
        uint32_t getBlockCount() const noexcept { return blocks.load (std::memory_order_acquire); }

        void reset() noexcept
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                deepestGrDb[ch].store (0.0f);
                currentGrDb[ch].store (0.0f);
            }
            inputPeak.store (0.0f);
            outputPeak.store (0.0f);
            limiterGrDb.store (0.0f);
            for (auto& b : bandGrDb)
                b.store (0.0f);
        }

    private:
        static void accumulateMin (std::atomic<float>& a, float v) noexcept
        {
            float prev = a.load (std::memory_order_relaxed);
            while (v < prev && ! a.compare_exchange_weak (prev, v, std::memory_order_acq_rel))
            {
            }
        }

        static void accumulateMax (std::atomic<float>& a, float v) noexcept
        {
            float prev = a.load (std::memory_order_relaxed);
            while (v > prev && ! a.compare_exchange_weak (prev, v, std::memory_order_acq_rel))
            {
            }
        }

        std::atomic<float> deepestGrDb[2] { 0.0f, 0.0f };
        std::atomic<float> currentGrDb[2] { 0.0f, 0.0f };
        std::atomic<float> thresholdDb { 0.0f }, ratio { 1.0f }, kneeDb { 0.0f }, makeupDb { 0.0f };
        std::atomic<float> attackMs { 0.0f }, releaseMs { 0.0f };
        std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
        std::atomic<int> oversamplingFactor { 1 };
        std::atomic<bool> colourActive { false };
        std::atomic<float> limiterGrDb { 0.0f };
        std::atomic<float> bandGrDb[3] { 0.0f, 0.0f, 0.0f };
        std::atomic<bool> multibandActive { false }, midSideActive { false };
        std::atomic<int> latencySamples { 0 };
        std::atomic<uint32_t> blocks { 0 };

        static_assert (std::atomic<float>::is_always_lock_free, "float atomics must be lock-free");
    };
}
