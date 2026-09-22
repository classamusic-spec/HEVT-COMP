#include "Core/Parameters.h"

namespace heat
{
    namespace
    {
        using Range = juce::NormalisableRange<float>;
        using APF = juce::AudioParameterFloat;
        using APC = juce::AudioParameterChoice;
        using APB = juce::AudioParameterBool;
        using Attr = juce::AudioParameterFloatAttributes;

        juce::String formatDb (float v, int)
        {
            if (std::abs (v) < 0.05f)
                return "0.0 dB";
            return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
        }

        juce::String formatMs (float v, int)
        {
            if (v < 1.0f)   return juce::String (v, 2) + " ms";
            if (v < 10.0f)  return juce::String (v, 1) + " ms";
            if (v < 1000.0f) return juce::String (juce::roundToInt (v)) + " ms";
            return juce::String (v / 1000.0f, 2) + " s";
        }

        juce::String formatHz (float v, int)
        {
            return juce::String (juce::roundToInt (v)) + " Hz";
        }

        juce::String formatPercent (float v, int)
        {
            return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        }

        float parseNumber (const juce::String& text)
        {
            return text.retainCharacters ("0123456789.-+").getFloatValue();
        }

        float parseMs (const juce::String& text)
        {
            const auto value = parseNumber (text);
            const auto t = text.trim().toLowerCase();
            if (t.endsWith ("s") && ! t.endsWith ("ms"))
                return value * 1000.0f;
            return value;
        }

        float parsePercent (const juce::String& text)
        {
            return juce::jlimit (0.0f, 1.0f, parseNumber (text) / 100.0f);
        }

        std::unique_ptr<APF> makeFloat (const char* id, const char* name, Range range, float def,
                                        juce::String (*toText) (float, int),
                                        float (*fromText) (const juce::String&),
                                        const char* label = "")
        {
            return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def,
                                          Attr().withLabel (label)
                                                .withStringFromValueFunction (toText)
                                                .withValueFromStringFunction (fromText));
        }
    }

    juce::NormalisableRange<float> makeLogRange (float minValue, float maxValue)
    {
        const auto ratio = maxValue / minValue;
        return { minValue, maxValue,
                 [] (float start, float end, float normalised)
                 {
                     return start * std::pow (end / start, normalised);
                 },
                 [] (float start, float end, float value)
                 {
                     return std::log (value / start) / std::log (end / start);
                 },
                 [ratio] (float start, float end, float value)
                 {
                     juce::ignoreUnused (ratio);
                     return juce::jlimit (start, end, value);
                 } };
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        const Range gainRange { ranges::gainMinDb, ranges::gainMaxDb, 0.01f };
        const Range unitRange { 0.0f, 1.0f, 0.0001f };

        layout.add (makeFloat (ids::input,  "Input",  gainRange, 0.0f, formatDb, parseNumber, "dB"));
        layout.add (makeFloat (ids::output, "Output", gainRange, 0.0f, formatDb, parseNumber, "dB"));

        layout.add (makeFloat (ids::compress, "Compress", unitRange, 0.5f, formatPercent, parsePercent, "%"));

        layout.add (makeFloat (ids::attack, "Attack",
                               makeLogRange (ranges::attackMinMs, ranges::attackMaxMs),
                               3.16f, formatMs, parseMs, "ms"));
        layout.add (makeFloat (ids::release, "Release",
                               makeLogRange (ranges::releaseMinMs, ranges::releaseMaxMs),
                               245.0f, formatMs, parseMs, "ms"));

        layout.add (std::make_unique<APC> (juce::ParameterID { ids::mode, 1 }, "Mode", modeChoices, 1));
        layout.add (std::make_unique<APC> (juce::ParameterID { ids::detector, 1 }, "Detector", detectorChoices, 0));

        layout.add (makeFloat (ids::tube, "Tube", unitRange, 0.0f, formatPercent, parsePercent, "%"));
        layout.add (makeFloat (ids::iron, "Iron", unitRange, 0.0f, formatPercent, parsePercent, "%"));
        layout.add (makeFloat (ids::mix,  "Mix",  unitRange, 1.0f, formatPercent, parsePercent, "%"));

        layout.add (makeFloat (ids::hpf, "SC HPF",
                               makeLogRange (ranges::hpfMinHz, ranges::hpfMaxHz),
                               ranges::hpfMinHz, formatHz, parseNumber, "Hz"));

        layout.add (std::make_unique<APC> (juce::ParameterID { ids::scSource, 1 }, "SC Source", scSourceChoices, 0));
        layout.add (std::make_unique<APC> (juce::ParameterID { ids::scLink, 1 }, "Stereo Link", linkChoices, 0));
        layout.add (std::make_unique<APB> (juce::ParameterID { ids::scListen, 1 }, "SC Listen", false));
        layout.add (std::make_unique<APB> (juce::ParameterID { ids::autoMakeup, 1 }, "Auto Makeup", true));
        layout.add (std::make_unique<APB> (juce::ParameterID { ids::autoRelease, 1 }, "Auto Release", false));
        layout.add (std::make_unique<APC> (juce::ParameterID { ids::quality, 1 }, "Quality", qualityChoices, 1));
        layout.add (std::make_unique<APB> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

        return layout;
    }

    ParameterCache::ParameterCache (juce::AudioProcessorValueTreeState& state)
    {
        auto get = [&state] (const char* id)
        {
            auto* p = state.getRawParameterValue (id);
            jassert (p != nullptr);
            return p;
        };

        input       = get (ids::input);
        output      = get (ids::output);
        compress    = get (ids::compress);
        attack      = get (ids::attack);
        release     = get (ids::release);
        mode        = get (ids::mode);
        detector    = get (ids::detector);
        tube        = get (ids::tube);
        iron        = get (ids::iron);
        mix         = get (ids::mix);
        hpf         = get (ids::hpf);
        scSource    = get (ids::scSource);
        scLink      = get (ids::scLink);
        scListen    = get (ids::scListen);
        autoMakeup  = get (ids::autoMakeup);
        autoRelease = get (ids::autoRelease);
        quality     = get (ids::quality);
        bypass      = get (ids::bypass);
    }

    dsp::EngineParams ParameterCache::read() const noexcept
    {
        auto choice = [] (const std::atomic<float>* p, int count)
        {
            return juce::jlimit (0, count - 1, juce::roundToInt (p->load (std::memory_order_relaxed)));
        };
        auto flag = [] (const std::atomic<float>* p)
        {
            return p->load (std::memory_order_relaxed) >= 0.5f;
        };
        auto value = [] (const std::atomic<float>* p)
        {
            return p->load (std::memory_order_relaxed);
        };

        dsp::EngineParams p;
        p.inputDb     = value (input);
        p.outputDb    = value (output);
        p.compress    = value (compress);
        p.attackMs    = value (attack);
        p.releaseMs   = value (release);
        p.mode        = static_cast<dsp::Mode> (choice (mode, dsp::numModes));
        p.detector    = static_cast<dsp::Detector> (choice (detector, dsp::numDetectors));
        p.tube        = value (tube);
        p.iron        = value (iron);
        p.mix         = value (mix);
        p.hpfHz       = value (hpf);
        p.scSource    = static_cast<dsp::SidechainSource> (choice (scSource, 2));
        p.link        = static_cast<dsp::StereoLink> (choice (scLink, 3));
        p.scListen    = flag (scListen);
        p.autoMakeup  = flag (autoMakeup);
        p.autoRelease = flag (autoRelease);
        p.quality     = static_cast<dsp::Quality> (choice (quality, 3));
        p.bypass      = flag (bypass);
        return p;
    }
}
