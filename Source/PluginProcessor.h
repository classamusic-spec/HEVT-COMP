#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/Parameters.h"
#include "Core/PluginState.h"
#include "Core/PresetManager.h"
#include "DSP/HeatEngine.h"
#include "UI/MeterTelemetry.h"

class HeatAudioProcessor final : public juce::AudioProcessor
{
public:
    // presetDirectory: override for tests; defaults to the user preset folder.
    explicit HeatAudioProcessor (juce::File presetDirectory = heat::PresetManager::getDefaultUserDirectory());
    ~HeatAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HEAT"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorParameter* getBypassParameter() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Editor-facing API (message thread) --------------------------------
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    heat::MeterTelemetry& getTelemetry() noexcept { return telemetry; }
    heat::PresetManager& getPresetManager() noexcept { return presets; }

    int getActiveABSlot() const noexcept { return ab.getActiveSlot(); }
    void switchABSlot (int slot);
    void copyABSlot (int from, int to);

    float getUiScale() const noexcept { return uiScale.load(); }
    void setUiScale (float s) noexcept { uiScale.store (juce::jlimit (0.5f, 2.0f, s)); }
    bool getPeakHold() const noexcept { return peakHold.load(); }
    void setPeakHold (bool b) noexcept { peakHold.store (b); }

    int getEngineLatency() const noexcept { return engine.getLatencySamples(); }

    static constexpr float defaultUiScale = 0.75f;

private:
    void runEngine (juce::AudioBuffer<float>& buffer, bool forceBypass);

    juce::UndoManager undoManager { 30000, 30 };
    juce::AudioProcessorValueTreeState state;
    heat::ParameterCache params;
    heat::dsp::HeatEngine engine;
    heat::MeterTelemetry telemetry;
    heat::PresetManager presets;
    heat::ABState ab;

    std::atomic<float> uiScale { defaultUiScale };
    std::atomic<bool> peakHold { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatAudioProcessor)
};
