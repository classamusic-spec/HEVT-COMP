#include "TestFramework.h"
#include "PluginTestUtils.h"

#include <atomic>
#include <cstdlib>
#include <new>
#include <random>
#include <thread>

// ---------------------------------------------------------------------------
// Allocation tracker: counts heap allocations made by the thread that has
// armed it (the simulated audio thread). Replaces global operator new/delete
// for this test executable only.
// ---------------------------------------------------------------------------
namespace
{
    thread_local bool trackingThisThread = false;
    std::atomic<long> trackedAllocations { 0 };

    struct ScopedAllocationTracking
    {
        ScopedAllocationTracking()  { trackingThisThread = true; }
        ~ScopedAllocationTracking() { trackingThisThread = false; }
    };

    void* allocate (std::size_t size)
    {
        if (trackingThisThread)
            trackedAllocations.fetch_add (1, std::memory_order_relaxed);
        if (void* p = std::malloc (size == 0 ? 1 : size))
            return p;
        throw std::bad_alloc();
    }
}

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    if (trackingThisThread)
        trackedAllocations.fetch_add (1, std::memory_order_relaxed);
    return std::malloc (size == 0 ? 1 : size);
}
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    if (trackingThisThread)
        trackedAllocations.fetch_add (1, std::memory_order_relaxed);
    return std::malloc (size == 0 ? 1 : size);
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace heat::test;

HEAT_TEST ("Realtime", "processBlock performs no heap allocation")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    CHECK (p.setBusesLayout (layout));
    p.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), 2048);
    juce::MidiBuffer midi;
    Noise noise (3);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    const auto ids = allParameterIds();

    long allocations = 0;
    for (int block = 0; block < 3000; ++block)
    {
        // Parameter changes happen outside the audio callback (host / UI).
        auto* param = p.getState().getParameter (ids[static_cast<size_t> (block) % ids.size()]);
        param->setValueNotifyingHost (uni (rng));
        if (block % 500 == 0)
            p.getPresetManager().loadPreset (block / 500);

        const int len = 1 + static_cast<int> (uni (rng) * 2047.0f);
        buffer.setSize (p.getTotalNumInputChannels(), len, false, false, true);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < len; ++i)
                buffer.setSample (ch, i, 0.7f * noise.next());

        trackedAllocations = 0;
        {
            ScopedAllocationTracking tracking;
            p.processBlock (buffer, midi);
        }
        allocations += trackedAllocations.load();
    }
    note ("heap allocations inside 3000 audio callbacks (all features exercised): %.0f", (double) allocations);
    CHECK (allocations == 0);
}

HEAT_TEST ("Realtime", "audio, UI and automation threads run concurrently")
{
    // Audio thread processes, a UI thread polls telemetry at "frame rate",
    // an automation thread writes parameters. Run under ThreadSanitizer for
    // data-race detection; in normal builds it checks for crashes / NaNs.
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    p.prepareToPlay (48000.0, 256);

    std::atomic<bool> running { true };
    std::atomic<bool> finiteOutput { true };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), 256);
        juce::MidiBuffer midi;
        Noise noise (11);
        for (int b = 0; b < 4000; ++b)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (ch, i, 0.6f * noise.next());
            p.processBlock (buffer, midi);
            for (int i = 0; i < 256; ++i)
                if (! std::isfinite (buffer.getSample (0, i)))
                    finiteOutput = false;
        }
        running = false;
    });

    std::thread ui ([&]
    {
        auto& t = p.getTelemetry();
        float sink = 0.0f;
        while (running)
        {
            sink += t.consumeDeepestGrDb (0) + t.getThresholdDb() + t.getRatio() + t.getCurrentGrDb (1);
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
        juce::ignoreUnused (sink);
    });

    std::thread automation ([&]
    {
        std::mt19937 rng (5);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        const auto ids = allParameterIds();
        while (running)
        {
            p.getState().getParameter (ids[static_cast<size_t> (uni (rng) * 17.0f)])->setValueNotifyingHost (uni (rng));
            std::this_thread::sleep_for (std::chrono::microseconds (300));
        }
    });

    audio.join();
    ui.join();
    automation.join();
    CHECK (finiteOutput.load());
}
