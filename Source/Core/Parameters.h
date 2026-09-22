#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/Constants.h"
#include "DSP/EngineParams.h"

namespace heat
{
    // Front-panel choice lists. Order is persisted (choice index) — append only.
    inline const juce::StringArray modeChoices     { "CLEAN", "WARM", "DRIVE" };
    inline const juce::StringArray detectorChoices { "PEAK", "RMS", "OPTICAL" };
    inline const juce::StringArray scSourceChoices { "INTERNAL", "EXTERNAL" };
    inline const juce::StringArray linkChoices     { "LINKED", "PARTIAL", "DUAL MONO" };
    inline const juce::StringArray qualityChoices  { "NORMAL", "HIGH", "ULTRA" };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Log-frequency / log-time mappings shared by the parameters and the UI.
    juce::NormalisableRange<float> makeLogRange (float minValue, float maxValue);

    // Realtime-safe cache of raw parameter pointers. Built once on the message
    // thread; read lock-free from the audio thread.
    class ParameterCache
    {
    public:
        explicit ParameterCache (juce::AudioProcessorValueTreeState& state);

        dsp::EngineParams read() const noexcept;

    private:
        std::atomic<float>* input       = nullptr;
        std::atomic<float>* output      = nullptr;
        std::atomic<float>* compress    = nullptr;
        std::atomic<float>* attack      = nullptr;
        std::atomic<float>* release     = nullptr;
        std::atomic<float>* mode        = nullptr;
        std::atomic<float>* detector    = nullptr;
        std::atomic<float>* tube        = nullptr;
        std::atomic<float>* iron        = nullptr;
        std::atomic<float>* mix         = nullptr;
        std::atomic<float>* hpf         = nullptr;
        std::atomic<float>* scSource    = nullptr;
        std::atomic<float>* scLink      = nullptr;
        std::atomic<float>* scListen    = nullptr;
        std::atomic<float>* autoMakeup  = nullptr;
        std::atomic<float>* autoRelease = nullptr;
        std::atomic<float>* quality     = nullptr;
        std::atomic<float>* bypass      = nullptr;
    };
}
