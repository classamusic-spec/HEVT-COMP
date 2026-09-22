#include "Core/PluginState.h"

namespace heat
{
    void ABState::capture (int slot, const juce::String& presetName)
    {
        slots[slot] = state.copyState();
        names[slot] = presetName;
    }

    juce::String ABState::switchTo (int slot, const juce::String& currentPresetName)
    {
        slot = juce::jlimit (0, 1, slot);
        if (slot == active)
            return currentPresetName;

        capture (active, currentPresetName);
        if (! slots[slot].isValid())
        {
            slots[slot] = slots[active].createCopy();
            names[slot] = names[active];
        }

        active = slot;
        state.replaceState (slots[slot].createCopy());
        return names[slot];
    }

    juce::String ABState::copy (int from, int to, const juce::String& currentPresetName)
    {
        from = juce::jlimit (0, 1, from);
        to = juce::jlimit (0, 1, to);
        if (from == to)
            return currentPresetName;

        capture (active, currentPresetName);
        slots[to] = slots[from].isValid() ? slots[from].createCopy() : state.copyState();
        names[to] = names[from];

        if (to == active)
        {
            state.replaceState (slots[to].createCopy());
            return names[to];
        }
        return currentPresetName;
    }

    juce::ValueTree ABState::toValueTree (const juce::String& currentPresetName) const
    {
        juce::ValueTree tree ("AB");
        tree.setProperty ("active", active, nullptr);
        for (int s = 0; s < 2; ++s)
        {
            juce::ValueTree slot ("SLOT");
            slot.setProperty ("index", s, nullptr);
            // The active slot is always saved from the live state.
            const auto snapshot = s == active ? state.copyState() : slots[s];
            slot.setProperty ("presetName", s == active ? currentPresetName : names[s], nullptr);
            if (snapshot.isValid())
                slot.appendChild (snapshot.createCopy(), nullptr);
            tree.appendChild (slot, nullptr);
        }
        return tree;
    }

    void ABState::fromValueTree (const juce::ValueTree& tree)
    {
        active = juce::jlimit (0, 1, static_cast<int> (tree.getProperty ("active", 0)));
        for (int s = 0; s < 2; ++s)
        {
            slots[s] = {};
            names[s] = {};
        }
        for (const auto& slot : tree)
        {
            const int index = juce::jlimit (0, 1, static_cast<int> (slot.getProperty ("index", 0)));
            names[index] = slot.getProperty ("presetName").toString();
            if (slot.getNumChildren() > 0)
                slots[index] = slot.getChild (0).createCopy();
        }
    }
}
