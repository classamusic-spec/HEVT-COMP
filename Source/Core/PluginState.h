#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace heat
{
    // A / B comparison slots. Each slot holds a full parameter snapshot and
    // the preset name it was showing. Message thread only.
    class ABState
    {
    public:
        explicit ABState (juce::AudioProcessorValueTreeState& s) : state (s) {}

        int getActiveSlot() const noexcept { return active; }

        // Switches to `slot`, storing the live state in the slot being left.
        // An empty target slot starts as a copy of the current one. Returns
        // the preset name associated with the new slot.
        juce::String switchTo (int slot, const juce::String& currentPresetName);

        // Copies one slot onto the other. Copying onto the active slot also
        // loads it.
        juce::String copy (int from, int to, const juce::String& currentPresetName);

        juce::ValueTree toValueTree (const juce::String& currentPresetName) const;
        void fromValueTree (const juce::ValueTree& tree);

        bool hasSlot (int slot) const { return slots[slot].isValid(); }

    private:
        void capture (int slot, const juce::String& presetName);

        juce::AudioProcessorValueTreeState& state;
        juce::ValueTree slots[2];
        juce::String names[2];
        int active = 0;
    };
}
