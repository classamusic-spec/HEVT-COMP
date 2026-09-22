#include "PluginEditor.h"
#include "PluginProcessor.h"

HeatAudioProcessorEditor::HeatAudioProcessorEditor (HeatAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1152, 768);
}

HeatAudioProcessorEditor::~HeatAudioProcessorEditor() = default;

void HeatAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffc4c6c9));
}

void HeatAudioProcessorEditor::resized() {}
