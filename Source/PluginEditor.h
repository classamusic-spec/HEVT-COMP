#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class HeatAudioProcessor;

class HeatAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HeatAudioProcessorEditor (HeatAudioProcessor&);
    ~HeatAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HeatAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatAudioProcessorEditor)
};
