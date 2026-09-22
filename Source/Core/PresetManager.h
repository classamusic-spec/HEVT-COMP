#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/FactoryPresets.h"

namespace heat
{
    // Factory + user presets, favourites and prev/next navigation.
    // Message thread only (never touched by the audio thread). Loading a
    // preset writes parameter values through the host-notifying API; the
    // engine's smoothers turn that into a click-free transition.
    class PresetManager final : public juce::ChangeBroadcaster
    {
    public:
        struct Entry
        {
            juce::String name;
            juce::String category;
            juce::String tags;
            bool isFactory = true;
            int factoryIndex = -1;
            juce::File file;

            juce::String key() const { return (isFactory ? "factory:" : "user:") + name; }
        };

        explicit PresetManager (juce::AudioProcessorValueTreeState& state,
                                juce::File userDirectory = getDefaultUserDirectory());

        static juce::File getDefaultUserDirectory();
        static constexpr const char* fileExtension = ".heatpreset";

        int getNumPresets() const noexcept { return static_cast<int> (entries.size()); }
        const Entry& getEntry (int index) const { return entries[static_cast<size_t> (index)]; }
        int findByName (const juce::String& name) const;

        int getCurrentIndex() const noexcept { return currentIndex; }
        juce::String getCurrentName() const;
        juce::String getCurrentTags() const;

        void loadPreset (int index);
        void loadNext();
        void loadPrevious();

        bool isFavourite (int index) const;
        void toggleFavourite (int index);

        bool saveUserPreset (const juce::String& name);
        bool deleteUserPreset (int index);
        void refreshUserPresets();

        // Restores the displayed preset after a session / A-B load without
        // touching parameter values.
        void setCurrentByName (const juce::String& name);

        // Serialises the current parameter values as a user-preset tree.
        juce::ValueTree createPresetTree (const juce::String& name) const;
        void applyPresetTree (const juce::ValueTree& preset);

    private:
        void applyFactory (const FactoryPreset& preset);
        void setParameter (const char* id, float plainValue);
        void loadFavourites();
        void saveFavourites() const;
        juce::File getFavouritesFile() const;

        juce::AudioProcessorValueTreeState& state;
        juce::File userDir;
        std::vector<Entry> entries;
        juce::StringArray favourites;
        int currentIndex = 0;
        juce::String currentName;
    };
}
