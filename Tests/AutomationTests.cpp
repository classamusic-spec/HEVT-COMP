#include "TestFramework.h"
#include "PluginTestUtils.h"

#include <random>

using namespace heat::test;

namespace
{
    struct BlockStats
    {
        bool finite = true;
        float peak = 0.0f;
    };

    BlockStats inspect (const juce::AudioBuffer<float>& b, int channels)
    {
        BlockStats s;
        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float v = b.getSample (ch, i);
                s.finite = s.finite && std::isfinite (v);
                s.peak = std::max (s.peak, std::abs (v));
            }
        return s;
    }
}

HEAT_TEST ("Automation", "fuzz: thousands of blocks of random automation")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    p.prepareToPlay (48000.0, 512);

    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    std::uniform_int_distribution<int> blockDist (1, 2048);
    const auto ids = allParameterIds();

    juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), 2048);
    juce::MidiBuffer midi;
    Noise noise (99);
    bool allFinite = true;
    float worstPeak = 0.0f;
    const int blocks = 6000;

    for (int b = 0; b < blocks; ++b)
    {
        // Automate 1-4 random parameters per block, sometimes jumping to extremes.
        const int changes = 1 + static_cast<int> (uni (rng) * 4.0f);
        for (int c = 0; c < changes; ++c)
        {
            auto* param = p.getState().getParameter (ids[static_cast<size_t> (uni (rng) * ids.size()) % ids.size()]);
            const float r = uni (rng);
            param->setValueNotifyingHost (r < 0.1f ? 0.0f : r > 0.9f ? 1.0f : uni (rng));
        }

        const int len = blockDist (rng);
        buffer.setSize (p.getTotalNumInputChannels(), len, false, false, true);
        const float level = uni (rng) < 0.05f ? 4.0f : 0.8f; // occasional very hot input
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < len; ++i)
                buffer.setSample (ch, i, level * noise.next());

        p.processBlock (buffer, midi);
        const auto s = inspect (buffer, 2);
        allFinite = allFinite && s.finite;
        worstPeak = std::max (worstPeak, s.peak);
    }
    note ("%.0f blocks fuzzed; all finite: %.0f; worst output peak %.2f", blocks, allFinite ? 1.0 : 0.0, worstPeak);
    CHECK (allFinite);
    CHECK (worstPeak <= 31.7f);
}

HEAT_TEST ("Automation", "sample rates x block sizes x layouts")
{
    TempPresetDir dir;
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (int bs : { 16, 32, 64, 128, 256, 512, 1024, 2048, 77 })
        {
            HeatAudioProcessor p (dir.dir);
            p.prepareToPlay (fs, bs);
            processNoise (p, static_cast<int> (fs * 0.25), bs);
            juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), bs);
            buffer.clear();
            buffer.setSample (0, 0, 0.5f);
            juce::MidiBuffer midi;
            p.processBlock (buffer, midi);
            const auto s = inspect (buffer, 2);
            CHECK (s.finite);
            CHECK (p.getLatencySamples() == p.getEngineLatency());
            CHECK (p.getLatencySamples() > 0 && p.getLatencySamples() < 200);
        }

    // Mono layout.
    HeatAudioProcessor mono (dir.dir);
    auto layout = mono.getBusesLayout();
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK (mono.setBusesLayout (layout));
    mono.prepareToPlay (48000.0, 256);
    processNoise (mono, 48000, 256);
    CHECK (mono.getTelemetry().getBlockCount() > 0);
}

HEAT_TEST ("Automation", "external sidechain through the plugin bus")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    CHECK (p.setBusesLayout (layout));
    setPlain (p, heat::ids::scSource, 1.0f);
    setPlain (p, heat::ids::compress, 0.6f);
    setPlain (p, heat::ids::mix, 1.0f);
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), 256);
    juce::MidiBuffer midi;
    float deepest = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float t = static_cast<float> (b * 256 + i) / 48000.0f;
            buffer.setSample (0, i, 0.02f * std::sin (6.2832f * 1000.0f * t));
            buffer.setSample (1, i, 0.02f * std::sin (6.2832f * 1000.0f * t));
            buffer.setSample (2, i, 0.8f * std::sin (6.2832f * 150.0f * t)); // key L
            buffer.setSample (3, i, 0.8f * std::sin (6.2832f * 150.0f * t)); // key R
        }
        p.processBlock (buffer, midi);
        deepest = std::min (deepest, p.getTelemetry().consumeDeepestGrDb (0));
    }
    note ("quiet programme keyed by loud sidechain: deepest GR %.2f dB", deepest);
    CHECK (deepest < -6.0f);
}

HEAT_TEST ("Automation", "multiple instances are independent")
{
    TempPresetDir dir;
    constexpr int totalSamples = 256 * 188; // whole number of 256-sample blocks
    auto render = [&] (HeatAudioProcessor& p, uint32_t seed)
    {
        juce::AudioBuffer<float> out (2, totalSamples);
        juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), 256);
        juce::MidiBuffer midi;
        Noise noise (seed);
        for (int s = 0; s < totalSamples; s += 256)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (ch, i, 0.5f * noise.next());
            p.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, s, buffer, ch, 0, 256);
        }
        return out;
    };

    HeatAudioProcessor soloA (dir.dir), soloB (dir.dir);
    setPlain (soloB, heat::ids::mode, 2.0f);
    setPlain (soloB, heat::ids::tube, 0.8f);
    soloA.prepareToPlay (48000.0, 256);
    soloB.prepareToPlay (48000.0, 256);
    const auto refA = render (soloA, 5);
    const auto refB = render (soloB, 6);

    // Interleaved processing of 8 instances (4 of each configuration).
    std::vector<std::unique_ptr<HeatAudioProcessor>> many;
    for (int i = 0; i < 8; ++i)
    {
        many.push_back (std::make_unique<HeatAudioProcessor> (dir.dir));
        if (i % 2 == 1)
        {
            setPlain (*many.back(), heat::ids::mode, 2.0f);
            setPlain (*many.back(), heat::ids::tube, 0.8f);
        }
        many.back()->prepareToPlay (48000.0, 256);
    }
    std::vector<juce::AudioBuffer<float>> outs (8, juce::AudioBuffer<float> (2, totalSamples));
    std::vector<Noise> noises;
    for (int i = 0; i < 8; ++i)
        noises.emplace_back (i % 2 == 0 ? 5u : 6u);
    juce::AudioBuffer<float> buffer (4, 256);
    juce::MidiBuffer midi;
    for (int s = 0; s < totalSamples; s += 256)
        for (int k = 0; k < 8; ++k)
        {
            buffer.setSize (many[static_cast<size_t> (k)]->getTotalNumInputChannels(), 256, false, false, true);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (ch, i, 0.5f * noises[static_cast<size_t> (k)].next());
            many[static_cast<size_t> (k)]->processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                outs[static_cast<size_t> (k)].copyFrom (ch, s, buffer, ch, 0, 256);
        }

    double worst = 0.0;
    for (int k = 0; k < 8; ++k)
    {
        const auto& ref = k % 2 == 0 ? refA : refB;
        for (int i = 0; i < totalSamples; ++i)
            worst = std::max (worst, (double) std::abs (outs[static_cast<size_t> (k)].getSample (0, i) - ref.getSample (0, i)));
    }
    note ("8 interleaved instances vs solo renders: max deviation %.2e", worst);
    CHECK (worst < 1.0e-6);
}

HEAT_TEST ("Automation", "editor open / close / resize stress")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    p.prepareToPlay (48000.0, 256);
    for (int i = 0; i < 25; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditorAndMakeActive());
        CHECK (editor != nullptr);
        processNoise (p, 2048, 256, static_cast<uint32_t> (i + 1));
        editor->setSize (768 + i * 16, (768 + i * 16) * 2 / 3);
        juce::Image snapshot (juce::Image::ARGB, 64, 64, true);
        juce::Graphics g (snapshot);
        editor->paintEntireComponent (g, false);
        p.editorBeingDeleted (editor.get());
    }
}
