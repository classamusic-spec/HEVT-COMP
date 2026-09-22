#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* stateRootType = "HEAT_STATE";
    constexpr const char* firstRunPreset = "Vocal Glue";
}

HeatAudioProcessor::HeatAudioProcessor (juce::File presetDirectory)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      state (*this, &undoManager, "HEAT", heat::createParameterLayout()),
      params (state),
      presets (state, std::move (presetDirectory)),
      ab (state)
{
    // A freshly inserted HEAT opens on the signature preset; hosts restoring a
    // session overwrite this through setStateInformation().
    presets.loadPreset (std::max (0, presets.findByName (firstRunPreset)));
    undoManager.clearUndoHistory();
}

HeatAudioProcessor::~HeatAudioProcessor() = default;

void HeatAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = std::max (1, getMainBusNumOutputChannels());
    engine.setParams (params.read());
    engine.prepare (sampleRate, samplesPerBlock, channels);
    setLatencySamples (engine.getLatencySamples());
    telemetry.reset();
}

void HeatAudioProcessor::releaseResources() {}

bool HeatAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void HeatAudioProcessor::runEngine (juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    auto main = getBusBuffer (buffer, true, 0);
    const int mainChannels = std::min (main.getNumChannels(), heat::dsp::HeatEngine::maxChannels);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (mainChannels == 0 || numSamples == 0)
        return;

    const float* scPtrs[2] { nullptr, nullptr };
    int scChannels = 0;
    if (getBusCount (true) > 1)
        if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            scChannels = std::min (sc.getNumChannels(), 2);
            for (int ch = 0; ch < scChannels; ++ch)
                scPtrs[ch] = sc.getReadPointer (ch);
        }

    auto p = params.read();
    if (forceBypass)
        p.bypass = true;
    engine.setParams (p);
    engine.process (main.getArrayOfWritePointers(), mainChannels,
                    scChannels > 0 ? scPtrs : nullptr, scChannels, numSamples);
    telemetry.publish (engine.getTelemetry());
}

void HeatAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    runEngine (buffer, false);
}

void HeatAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Keep latency and a click-free cross-fade even when the host bypasses
    // without using HEAT's own bypass parameter.
    runEngine (buffer, true);
}

juce::AudioProcessorParameter* HeatAudioProcessor::getBypassParameter() const
{
    return state.getParameter (heat::ids::bypass);
}

juce::AudioProcessorEditor* HeatAudioProcessor::createEditor()
{
    return new HeatAudioProcessorEditor (*this);
}

// --- Programs (host preset menus) ---------------------------------------------

int HeatAudioProcessor::getNumPrograms()
{
    return std::max (1, static_cast<int> (heat::getFactoryPresets().size()));
}

int HeatAudioProcessor::getCurrentProgram()
{
    const int idx = presets.getCurrentIndex();
    return idx < static_cast<int> (heat::getFactoryPresets().size()) ? idx : 0;
}

void HeatAudioProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < static_cast<int> (heat::getFactoryPresets().size()))
        presets.loadPreset (index);
}

const juce::String HeatAudioProcessor::getProgramName (int index)
{
    const auto& factory = heat::getFactoryPresets();
    if (index >= 0 && index < static_cast<int> (factory.size()))
        return juce::String::fromUTF8 (factory[static_cast<size_t> (index)].name);
    return {};
}

// --- A / B ------------------------------------------------------------------------

void HeatAudioProcessor::switchABSlot (int slot)
{
    const auto name = ab.switchTo (slot, presets.getCurrentName());
    presets.setCurrentByName (name);
}

void HeatAudioProcessor::copyABSlot (int from, int to)
{
    const auto name = ab.copy (from, to, presets.getCurrentName());
    presets.setCurrentByName (name);
}

// --- State ------------------------------------------------------------------------

void HeatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root (stateRootType);
    root.setProperty ("version", heat::stateVersion, nullptr);
    root.setProperty ("presetName", presets.getCurrentName(), nullptr);
    root.setProperty ("uiScale", uiScale.load(), nullptr);
    root.setProperty ("peakHold", peakHold.load(), nullptr);
    root.appendChild (state.copyState(), nullptr);
    root.appendChild (ab.toValueTree (presets.getCurrentName()), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void HeatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto root = juce::ValueTree::fromXml (*xml);

    // Legacy (pre-release) sessions stored the bare parameter tree.
    if (root.hasType (state.state.getType()))
    {
        state.replaceState (root);
        return;
    }

    if (! root.hasType (stateRootType))
        return;

    if (auto paramTree = root.getChildWithName (state.state.getType()); paramTree.isValid())
        state.replaceState (paramTree.createCopy());

    if (auto abTree = root.getChildWithName ("AB"); abTree.isValid())
        ab.fromValueTree (abTree);

    uiScale.store (juce::jlimit (0.5f, 2.0f, static_cast<float> (root.getProperty ("uiScale", defaultUiScale))));
    peakHold.store (static_cast<bool> (root.getProperty ("peakHold", true)));
    presets.setCurrentByName (root.getProperty ("presetName").toString());
    undoManager.clearUndoHistory();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeatAudioProcessor();
}
