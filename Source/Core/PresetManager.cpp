#include "Core/PresetManager.h"
#include "Core/Constants.h"

namespace heat
{
    namespace
    {
        // Parameters a preset stores. Session-level settings (quality,
        // sidechain source, listen, bypass) are deliberately not part of a
        // preset so browsing never changes routing or CPU cost.
        const char* const presetParameterIds[] = {
            ids::input, ids::output, ids::compress, ids::attack, ids::release, ids::mode, ids::detector,
            ids::tube, ids::iron, ids::mix, ids::hpf, ids::scLink, ids::autoMakeup, ids::autoRelease
        };
    }

    juce::File PresetManager::getDefaultUserDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Nova Audio").getChildFile ("HEAT").getChildFile ("Presets");
    }

    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, juce::File userDirectory)
        : state (s), userDir (std::move (userDirectory))
    {
        refreshUserPresets();
        loadFavourites();
    }

    void PresetManager::refreshUserPresets()
    {
        const auto previousName = getCurrentName();
        entries.clear();

        const auto& factory = getFactoryPresets();
        for (int i = 0; i < static_cast<int> (factory.size()); ++i)
        {
            Entry e;
            e.name = juce::String::fromUTF8 (factory[static_cast<size_t> (i)].name);
            e.category = factory[static_cast<size_t> (i)].category;
            e.tags = juce::String::fromUTF8 (factory[static_cast<size_t> (i)].tags);
            e.isFactory = true;
            e.factoryIndex = i;
            entries.push_back (e);
        }

        if (userDir.isDirectory())
        {
            auto files = userDir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
            files.sort();
            for (const auto& f : files)
            {
                if (auto xml = juce::XmlDocument::parse (f))
                {
                    Entry e;
                    e.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
                    e.category = "USER";
                    e.tags = xml->getStringAttribute ("tags", "USER");
                    e.isFactory = false;
                    e.file = f;
                    entries.push_back (e);
                }
            }
        }

        const int found = findByName (previousName);
        currentIndex = found >= 0 ? found : juce::jlimit (0, getNumPresets() - 1, currentIndex);
    }

    int PresetManager::findByName (const juce::String& name) const
    {
        for (int i = 0; i < getNumPresets(); ++i)
            if (entries[static_cast<size_t> (i)].name == name)
                return i;
        return -1;
    }

    juce::String PresetManager::getCurrentName() const
    {
        if (currentName.isNotEmpty())
            return currentName;
        return entries.empty() ? juce::String() : entries[static_cast<size_t> (currentIndex)].name;
    }

    juce::String PresetManager::getCurrentTags() const
    {
        const int idx = findByName (getCurrentName());
        return idx >= 0 ? entries[static_cast<size_t> (idx)].tags : juce::String();
    }

    void PresetManager::setParameter (const char* id, float plainValue)
    {
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    }

    void PresetManager::applyFactory (const FactoryPreset& p)
    {
        setParameter (ids::input, p.inputDb);
        setParameter (ids::output, p.outputDb);
        setParameter (ids::compress, p.compress / 100.0f);
        setParameter (ids::attack, p.attackMs);
        setParameter (ids::release, p.releaseMs);
        setParameter (ids::mode, static_cast<float> (p.mode));
        setParameter (ids::detector, static_cast<float> (p.detector));
        setParameter (ids::tube, p.tube / 100.0f);
        setParameter (ids::iron, p.iron / 100.0f);
        setParameter (ids::mix, p.mix / 100.0f);
        setParameter (ids::hpf, p.hpfHz);
        setParameter (ids::scLink, static_cast<float> (p.link));
        setParameter (ids::autoMakeup, 1.0f);
        setParameter (ids::autoRelease, 0.0f);
    }

    juce::ValueTree PresetManager::createPresetTree (const juce::String& name) const
    {
        juce::ValueTree tree ("HEAT_PRESET");
        tree.setProperty ("name", name, nullptr);
        tree.setProperty ("version", stateVersion, nullptr);
        tree.setProperty ("tags", "USER", nullptr);
        for (auto* id : presetParameterIds)
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            {
                juce::ValueTree param ("PARAM");
                param.setProperty ("id", id, nullptr);
                param.setProperty ("value", p->convertFrom0to1 (p->getValue()), nullptr);
                tree.appendChild (param, nullptr);
            }
        return tree;
    }

    void PresetManager::applyPresetTree (const juce::ValueTree& preset)
    {
        for (const auto& param : preset)
            if (param.hasType ("PARAM"))
            {
                const auto id = param.getProperty ("id").toString();
                if (auto* p = state.getParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (param.getProperty ("value"))));
            }
    }

    void PresetManager::loadPreset (int index)
    {
        if (index < 0 || index >= getNumPresets())
            return;

        const auto& e = entries[static_cast<size_t> (index)];
        if (e.isFactory)
        {
            applyFactory (getFactoryPresets()[static_cast<size_t> (e.factoryIndex)]);
        }
        else if (auto xml = juce::XmlDocument::parse (e.file))
        {
            applyPresetTree (juce::ValueTree::fromXml (*xml));
        }

        currentIndex = index;
        currentName = e.name;
        sendChangeMessage();
    }

    void PresetManager::loadNext()
    {
        if (getNumPresets() > 0)
            loadPreset ((currentIndex + 1) % getNumPresets());
    }

    void PresetManager::loadPrevious()
    {
        if (getNumPresets() > 0)
            loadPreset ((currentIndex + getNumPresets() - 1) % getNumPresets());
    }

    void PresetManager::setCurrentByName (const juce::String& name)
    {
        const int idx = findByName (name);
        if (idx >= 0)
            currentIndex = idx;
        currentName = name;
        sendChangeMessage();
    }

    bool PresetManager::isFavourite (int index) const
    {
        return index >= 0 && index < getNumPresets() && favourites.contains (entries[static_cast<size_t> (index)].key());
    }

    void PresetManager::toggleFavourite (int index)
    {
        if (index < 0 || index >= getNumPresets())
            return;
        const auto key = entries[static_cast<size_t> (index)].key();
        if (favourites.contains (key))
            favourites.removeString (key);
        else
            favourites.add (key);
        saveFavourites();
        sendChangeMessage();
    }

    bool PresetManager::saveUserPreset (const juce::String& rawName)
    {
        const auto name = rawName.trim();
        if (name.isEmpty() || ! userDir.createDirectory())
            return false;

        const auto file = userDir.getChildFile (juce::File::createLegalFileName (name) + fileExtension);
        const auto tree = createPresetTree (name);
        if (auto xml = tree.createXml(); xml == nullptr || ! xml->writeTo (file))
            return false;

        refreshUserPresets();
        currentIndex = std::max (0, findByName (name));
        currentName = name;
        sendChangeMessage();
        return true;
    }

    bool PresetManager::deleteUserPreset (int index)
    {
        if (index < 0 || index >= getNumPresets() || entries[static_cast<size_t> (index)].isFactory)
            return false;
        const bool ok = entries[static_cast<size_t> (index)].file.deleteFile();
        refreshUserPresets();
        sendChangeMessage();
        return ok;
    }

    juce::File PresetManager::getFavouritesFile() const
    {
        return userDir.getParentDirectory().getChildFile ("favourites.xml");
    }

    void PresetManager::loadFavourites()
    {
        favourites.clear();
        if (auto xml = juce::XmlDocument::parse (getFavouritesFile()))
            for (auto* f : xml->getChildIterator())
                favourites.addIfNotAlreadyThere (f->getStringAttribute ("key"));
    }

    void PresetManager::saveFavourites() const
    {
        juce::XmlElement root ("FAVOURITES");
        for (const auto& key : favourites)
            root.createNewChildElement ("F")->setAttribute ("key", key);
        getFavouritesFile().getParentDirectory().createDirectory();
        root.writeTo (getFavouritesFile());
    }
}
