#pragma once

#include "PluginProcessor.h"
#include "TestSignals.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace heat::test
{
    // A throwaway preset directory per test so user files are never touched.
    struct TempPresetDir
    {
        juce::TemporaryFile temp { "heat_presets" };
        juce::File dir = temp.getFile();

        TempPresetDir() { dir.createDirectory(); }
        ~TempPresetDir() { dir.getParentDirectory().getChildFile ("favourites.xml").deleteFile(); dir.deleteRecursively(); }
    };

    inline float getPlain (HeatAudioProcessor& p, const char* id)
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    }

    inline void setPlain (HeatAudioProcessor& p, const char* id, float value)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    inline std::vector<const char*> allParameterIds()
    {
        using namespace heat::ids;
        return { input, output, compress, attack, release, mode, detector, tube, iron, mix, hpf,
                 scSource, scLink, scListen, autoMakeup, autoRelease, quality, bypass,
                 stereoMode, lookahead, scLpf, scEqFreq, scEqGain, scEqQ, limiter, ceiling, ironModel,
                 multiband, xoverLow, xoverHigh, bandLow, bandMid, bandHigh };
    }

    inline void processNoise (HeatAudioProcessor& p, int numSamples, int blockSize, uint32_t seed = 1, float gain = 0.5f)
    {
        juce::AudioBuffer<float> buffer (p.getTotalNumInputChannels(), blockSize);
        juce::MidiBuffer midi;
        Noise noise (seed);
        for (int done = 0; done < numSamples; done += blockSize)
        {
            const int len = std::min (blockSize, numSamples - done);
            buffer.setSize (p.getTotalNumInputChannels(), len, false, false, true);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < len; ++i)
                    buffer.setSample (ch, i, gain * noise.next());
            p.processBlock (buffer, midi);
        }
    }
}
