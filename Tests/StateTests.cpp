#include "TestFramework.h"
#include "PluginTestUtils.h"

#include <random>
#include <set>

using namespace heat::test;

HEAT_TEST ("State", "parameter IDs are stable")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    const char* expected[] = {
        "heat.input.v2", "heat.output.v2", "heat.compress.v2", "heat.attack.v2", "heat.release.v2",
        "heat.mode.v2", "heat.detector.v2", "heat.tube.v2", "heat.iron.v2", "heat.mix.v2",
        "heat.sidechain.hpf.v2", "heat.sidechain.source.v2", "heat.sidechain.link.v2",
        "heat.sidechain.listen.v2", "heat.autoMakeup.v2", "heat.autoRelease.v2", "heat.quality.v2", "heat.bypass.v2",
        // 2.1 (appended)
        "heat.stereo.mode.v2", "heat.lookahead.v2", "heat.sidechain.lpf.v2", "heat.sidechain.eq.freq.v2",
        "heat.sidechain.eq.gain.v2", "heat.sidechain.eq.q.v2", "heat.limiter.v2", "heat.limiter.ceiling.v2",
        "heat.iron.model.v2", "heat.multiband.v2", "heat.multiband.xover.low.v2", "heat.multiband.xover.high.v2",
        "heat.multiband.low.v2", "heat.multiband.mid.v2", "heat.multiband.high.v2"
    };
    for (auto* id : expected)
        CHECK (p.getState().getParameter (id) != nullptr);
    CHECK (p.getParameters().size() == static_cast<int> (std::size (expected)));

    // Host parameter order is append-only: 2.0 indices are unchanged.
    for (int i = 0; i < p.getParameters().size(); ++i)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p.getParameters()[i]))
            CHECK (withId->paramID == expected[i]);

    // Latency-changing controls are not automatable.
    CHECK (! p.getState().getParameter (heat::ids::lookahead)->isAutomatable());
    CHECK (! p.getState().getParameter (heat::ids::limiter)->isAutomatable());
}

HEAT_TEST ("State", "2.0 sessions and presets keep the CLASSIC IRON; new controls start neutral")
{
    TempPresetDir dir;

    // A fresh 2.1 instance uses the hysteresis core.
    HeatAudioProcessor fresh (dir.dir);
    CHECK (juce::roundToInt (getPlain (fresh, heat::ids::ironModel)) == 1);

    // Build a 2.0 session: version 2, no 2.1 parameters in the trees.
    HeatAudioProcessor source (dir.dir);
    setPlain (source, heat::ids::iron, 0.6f);
    setPlain (source, heat::ids::compress, 0.42f);
    auto paramTree = source.getState().copyState();
    for (auto* id : { heat::ids::stereoMode, heat::ids::lookahead, heat::ids::scLpf, heat::ids::scEqFreq,
                      heat::ids::scEqGain, heat::ids::scEqQ, heat::ids::limiter, heat::ids::ceiling,
                      heat::ids::ironModel, heat::ids::multiband, heat::ids::xoverLow, heat::ids::xoverHigh,
                      heat::ids::bandLow, heat::ids::bandMid, heat::ids::bandHigh })
        paramTree.removeChild (paramTree.getChildWithProperty ("id", id), nullptr);
    juce::ValueTree root ("HEAT_STATE");
    root.setProperty ("version", 2, nullptr);
    root.appendChild (paramTree.createCopy(), nullptr);
    juce::ValueTree ab ("AB");
    ab.setProperty ("active", 0, nullptr);
    for (int s = 0; s < 2; ++s)
    {
        juce::ValueTree slot ("SLOT");
        slot.setProperty ("index", s, nullptr);
        slot.appendChild (paramTree.createCopy(), nullptr);
        ab.appendChild (slot, nullptr);
    }
    root.appendChild (ab, nullptr);
    juce::MemoryBlock block;
    if (auto xml = root.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, block);

    HeatAudioProcessor loaded (dir.dir);
    setPlain (loaded, heat::ids::multiband, 2.0f);   // must be reset by the load
    setPlain (loaded, heat::ids::limiter, 1.0f);
    loaded.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::ironModel)) == 0);
    CHECK_NEAR (getPlain (loaded, heat::ids::iron), 0.6, 1.0e-4);
    CHECK_NEAR (getPlain (loaded, heat::ids::compress), 0.42, 1.0e-4);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::multiband)) == 0);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::limiter)) == 0);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::lookahead)) == 0);

    // The B slot (also 2.0) is migrated too.
    loaded.switchABSlot (1);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::ironModel)) == 0);

    // A 2.1 round trip keeps HYSTERESIS.
    juce::MemoryBlock current;
    fresh.getStateInformation (current);
    HeatAudioProcessor again (dir.dir);
    again.setStateInformation (current.getData(), static_cast<int> (current.getSize()));
    CHECK (juce::roundToInt (getPlain (again, heat::ids::ironModel)) == 1);

    // 2.0 user preset: missing 2.1 controls reset to neutral, IRON stays CLASSIC.
    auto& pm = loaded.getPresetManager();
    auto presetTree = pm.createPresetTree ("Old One");
    presetTree.setProperty ("version", 2, nullptr);
    for (auto* id : { heat::ids::ironModel, heat::ids::multiband, heat::ids::limiter, heat::ids::scEqGain })
        presetTree.removeChild (presetTree.getChildWithProperty ("id", id), nullptr);
    setPlain (loaded, heat::ids::ironModel, 1.0f);
    setPlain (loaded, heat::ids::multiband, 1.0f);
    setPlain (loaded, heat::ids::limiter, 1.0f);
    setPlain (loaded, heat::ids::scEqGain, 9.0f);
    pm.applyPresetTree (presetTree);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::ironModel)) == 0);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::multiband)) == 0);
    CHECK (juce::roundToInt (getPlain (loaded, heat::ids::limiter)) == 0);
    CHECK_NEAR (getPlain (loaded, heat::ids::scEqGain), 0.0, 1.0e-4);
}

HEAT_TEST ("State", "reported latency follows LOOKAHEAD and LIMITER")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    p.prepareToPlay (48000.0, 512);
    const int core = p.getLatencySamples();
    CHECK (core == p.getEngineLatency());
    setPlain (p, heat::ids::lookahead, 4.0f);   // 5 ms
    CHECK (p.getLatencySamples() == core + 240);
    setPlain (p, heat::ids::limiter, 1.0f);
    CHECK (p.getLatencySamples() > core + 240);
    const int withBoth = p.getLatencySamples();
    processNoise (p, 512, 512);                  // the engine picks the settings up
    CHECK (p.getLatencySamples() == p.getEngineLatency());

    // Loading a preset that turns both off brings it back.
    p.getPresetManager().loadPreset (p.getPresetManager().findByName ("Vocal Glue"));
    CHECK (p.getLatencySamples() == core);

    // A session restore updates it as well.
    setPlain (p, heat::ids::lookahead, 4.0f);
    setPlain (p, heat::ids::limiter, 1.0f);
    juce::MemoryBlock block;
    p.getStateInformation (block);
    HeatAudioProcessor q (dir.dir);
    q.prepareToPlay (48000.0, 512);
    q.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (q.getLatencySamples() == withBoth);
    note ("48 kHz latency: core %.0f, +5 ms lookahead + limiter %.0f samples", core, withBoth);
}

HEAT_TEST ("State", "session round trip restores every parameter")
{
    TempPresetDir dir;
    std::mt19937 rng (42);
    for (int trial = 0; trial < 20; ++trial)
    {
        HeatAudioProcessor a (dir.dir);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        for (auto* id : allParameterIds())
            a.getState().getParameter (id)->setValueNotifyingHost (uni (rng));
        a.setUiScale (0.5f + uni (rng));

        juce::MemoryBlock block;
        a.getStateInformation (block);

        HeatAudioProcessor b (dir.dir);
        b.setStateInformation (block.getData(), static_cast<int> (block.getSize()));

        // Compare plain (user-unit) values: choice and bool parameters snap
        // to their steps, which is exactly what a host sees.
        for (auto* id : allParameterIds())
        {
            auto* pa = dynamic_cast<juce::RangedAudioParameter*> (a.getState().getParameter (id));
            const bool stepped = pa->getNumSteps() < 100;
            const float va = getPlain (a, id), vb = getPlain (b, id);
            if (stepped)
                CHECK (juce::roundToInt (va) == juce::roundToInt (vb));
            else
                CHECK_NEAR (vb, va, 1.0e-4 * std::max (1.0f, std::abs (va)));
        }
        CHECK_NEAR (b.getUiScale(), a.getUiScale(), 1.0e-5);
        CHECK (b.getPresetManager().getCurrentName() == a.getPresetManager().getCurrentName());
    }
}

HEAT_TEST ("State", "first instantiation shows Vocal Glue at the reference positions")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    CHECK (p.getPresetManager().getCurrentName() == "Vocal Glue");
    CHECK (p.getPresetManager().getCurrentTags().contains ("SMOOTH"));
    // Every continuous control sits at 12 o'clock like the locked reference.
    for (auto* id : { heat::ids::input, heat::ids::output, heat::ids::compress, heat::ids::attack,
                      heat::ids::release, heat::ids::tube, heat::ids::iron, heat::ids::mix, heat::ids::hpf })
        CHECK_NEAR (p.getState().getParameter (id)->getValue(), 0.5, 0.01);
    CHECK (juce::roundToInt (getPlain (p, heat::ids::mode)) == 1);     // WARM
    CHECK (juce::roundToInt (getPlain (p, heat::ids::detector)) == 0); // PEAK
}

HEAT_TEST ("State", "A/B switching, copying and persistence")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    setPlain (p, heat::ids::compress, 0.2f);
    setPlain (p, heat::ids::tube, 0.1f);

    p.switchABSlot (1); // B starts as a copy of A
    CHECK (p.getActiveABSlot() == 1);
    CHECK_NEAR (getPlain (p, heat::ids::compress), 0.2, 1.0e-4);
    setPlain (p, heat::ids::compress, 0.9f);
    setPlain (p, heat::ids::mode, 2.0f);

    p.switchABSlot (0);
    CHECK_NEAR (getPlain (p, heat::ids::compress), 0.2, 1.0e-4);
    CHECK (juce::roundToInt (getPlain (p, heat::ids::mode)) == 1);

    p.switchABSlot (1);
    CHECK_NEAR (getPlain (p, heat::ids::compress), 0.9, 1.0e-4);
    CHECK (juce::roundToInt (getPlain (p, heat::ids::mode)) == 2);

    // Session save/restore keeps both slots and the active one.
    juce::MemoryBlock block;
    p.getStateInformation (block);
    HeatAudioProcessor q (dir.dir);
    q.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK (q.getActiveABSlot() == 1);
    CHECK_NEAR (getPlain (q, heat::ids::compress), 0.9, 1.0e-4);
    q.switchABSlot (0);
    CHECK_NEAR (getPlain (q, heat::ids::compress), 0.2, 1.0e-4);

    // Copy B → A while A is active loads B's values.
    q.copyABSlot (1, 0);
    CHECK_NEAR (getPlain (q, heat::ids::compress), 0.9, 1.0e-4);
}

HEAT_TEST ("State", "factory presets: count, uniqueness, categories, loading")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    auto& pm = p.getPresetManager();
    const auto& factory = heat::getFactoryPresets();
    note ("factory presets: %.0f", (double) factory.size());
    CHECK (factory.size() >= 30);

    std::set<std::string> names;
    for (const auto& f : factory)
    {
        names.insert (f.name);
        bool knownCategory = false;
        for (auto* c : heat::getPresetCategories())
            knownCategory = knownCategory || std::string (c) == f.category;
        CHECK (knownCategory);
        CHECK (juce::String::fromUTF8 (f.tags).isNotEmpty());
    }
    CHECK (names.size() == factory.size());

    for (int i = 0; i < static_cast<int> (factory.size()); ++i)
    {
        pm.loadPreset (i);
        const auto& f = factory[static_cast<size_t> (i)];
        CHECK (pm.getCurrentName() == juce::String::fromUTF8 (f.name));
        CHECK_NEAR (getPlain (p, heat::ids::compress), f.compress / 100.0f, 1.0e-3);
        CHECK_NEAR (getPlain (p, heat::ids::attack), f.attackMs, 0.01 * f.attackMs + 1.0e-3);
        CHECK_NEAR (getPlain (p, heat::ids::release), f.releaseMs, 0.01 * f.releaseMs);
        CHECK (juce::roundToInt (getPlain (p, heat::ids::mode)) == f.mode);
        CHECK (juce::roundToInt (getPlain (p, heat::ids::detector)) == f.detector);
        CHECK_NEAR (getPlain (p, heat::ids::mix), f.mix / 100.0f, 1.0e-3);
    }

    // Navigation wraps in both directions.
    pm.loadPreset (0);
    pm.loadPrevious();
    CHECK (pm.getCurrentIndex() == pm.getNumPresets() - 1);
    pm.loadNext();
    CHECK (pm.getCurrentIndex() == 0);
}

HEAT_TEST ("State", "user presets and favourites persist")
{
    TempPresetDir dir;
    {
        HeatAudioProcessor p (dir.dir);
        auto& pm = p.getPresetManager();
        setPlain (p, heat::ids::compress, 0.77f);
        setPlain (p, heat::ids::release, 1234.0f);
        CHECK (pm.saveUserPreset ("My Vocal"));
        pm.toggleFavourite (pm.findByName ("Drum Clamp"));
        CHECK (pm.isFavourite (pm.findByName ("Drum Clamp")));
    }
    {
        HeatAudioProcessor p (dir.dir);
        auto& pm = p.getPresetManager();
        const int idx = pm.findByName ("My Vocal");
        CHECK (idx >= 0);
        CHECK (pm.isFavourite (pm.findByName ("Drum Clamp")));
        setPlain (p, heat::ids::compress, 0.1f);
        pm.loadPreset (idx);
        CHECK_NEAR (getPlain (p, heat::ids::compress), 0.77, 1.0e-3);
        CHECK_NEAR (getPlain (p, heat::ids::release), 1234.0, 1.0);
        CHECK (pm.deleteUserPreset (idx));
        CHECK (pm.findByName ("My Vocal") < 0);
    }
}

HEAT_TEST ("State", "legacy and corrupt state data are handled")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);

    // Legacy: bare parameter tree.
    auto legacy = p.getState().copyState();
    legacy.getChildWithProperty ("id", heat::ids::compress).setProperty ("value", 0.33f, nullptr);
    juce::MemoryBlock block;
    if (auto xml = legacy.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, block);
    p.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    CHECK_NEAR (getPlain (p, heat::ids::compress), 0.33, 1.0e-4);

    // Garbage must not crash or change anything.
    const char garbage[] = "not a heat state at all \x01\x02\x03";
    p.setStateInformation (garbage, sizeof (garbage));
    p.setStateInformation (nullptr, 0);
    CHECK_NEAR (getPlain (p, heat::ids::compress), 0.33, 1.0e-4);
}

HEAT_TEST ("State", "undo restores parameter edits")
{
    TempPresetDir dir;
    HeatAudioProcessor p (dir.dir);
    auto& um = p.getUndoManager();
    const float before = getPlain (p, heat::ids::compress);

    um.beginNewTransaction();
    auto* param = p.getState().getParameter (heat::ids::compress);
    param->beginChangeGesture();
    param->setValueNotifyingHost (0.95f);
    param->endChangeGesture();
    // APVTS mirrors parameters into its tree on a timer; flush synchronously.
    p.getState().copyState();
    um.beginNewTransaction();

    CHECK_NEAR (getPlain (p, heat::ids::compress), 0.95, 1.0e-4);
    CHECK (um.canUndo());
    um.undo();
    CHECK_NEAR (getPlain (p, heat::ids::compress), before, 1.0e-4);
}
