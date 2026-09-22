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
    inline const juce::StringArray stereoModeChoices { "L/R", "M/S", "MID", "SIDE" };
    inline const juce::StringArray lookaheadChoices  { "OFF", "0.5 ms", "1 ms", "2 ms", "5 ms", "10 ms" };
    inline const juce::StringArray ironModelChoices  { "CLASSIC", "HYSTERESIS" };
    inline const juce::StringArray multibandChoices  { "OFF", "2 BAND", "3 BAND" };

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

        std::atomic<float>* stereoMode  = nullptr;
        std::atomic<float>* lookahead   = nullptr;
        std::atomic<float>* scLpf       = nullptr;
        std::atomic<float>* scEqFreq    = nullptr;
        std::atomic<float>* scEqGain    = nullptr;
        std::atomic<float>* scEqQ       = nullptr;
        std::atomic<float>* limiter     = nullptr;
        std::atomic<float>* ceiling     = nullptr;
        std::atomic<float>* ironModel   = nullptr;
        std::atomic<float>* multiband   = nullptr;
        std::atomic<float>* xoverLow    = nullptr;
        std::atomic<float>* xoverHigh   = nullptr;
        std::atomic<float>* bandLow     = nullptr;
        std::atomic<float>* bandMid     = nullptr;
        std::atomic<float>* bandHigh    = nullptr;
    };

    // LOOKAHEAD choice index → milliseconds.
    float lookaheadMsForIndex (int index) noexcept;
}
