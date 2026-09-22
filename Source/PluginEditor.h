#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "UI/AdvancedPanel.h"
#include "UI/GainReductionDisplay.h"
#include "UI/Header.h"
#include "UI/HeatKnob.h"
#include "UI/HeatLookAndFeel.h"
#include "UI/SegmentedSelector.h"

class HeatAudioProcessor;

namespace heat::ui
{
    // Floating readout shown while a knob is touched.
    class ValueBubble : public juce::Component
    {
    public:
        ValueBubble() { setInterceptsMouseClicks (false, false); setAlwaysOnTop (true); }
        void show (const juce::String& title, const juce::String& value, const juce::String& detail,
                   juce::Point<float> anchor, float anchorRadius);
        void paint (juce::Graphics& g) override;

    private:
        juce::String title, value, detail;
    };

    // The 1536 x 1024 design canvas holding the whole instrument.
    class MainPanel : public juce::Component
    {
    public:
        explicit MainPanel (HeatAudioProcessor& processor);
        ~MainPanel() override;

        void paint (juce::Graphics& g) override;
        void resized() override {}

        void onFrame (double elapsedSeconds);
        void setPreviewGainReduction (float db);   // snapshots / previews
        void toggleAdvanced();

        std::function<void (float)> onUiScaleRequest;

    private:
        class Scrim;
        void knobInteraction (HeatKnob& knob, bool active);
        void updateBubble();
        juce::String compressDetail() const;

        HeatAudioProcessor& processor;
        juce::Image chassis;
        float chassisScale = 0.0f;

        Header header;
        GainReductionDisplay meter;
        HeatKnob input, output, attack, release, compress, tube, iron, mix, hpf;
        SegmentedSelector mode, detector;
        std::unique_ptr<Scrim> scrim;
        AdvancedPanel advanced;
        ValueBubble bubble;

        HeatKnob* activeKnob = nullptr;
        juce::uint32 lastBlocks = 0;
    };
}

class HeatAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HeatAudioProcessorEditor (HeatAudioProcessor&);
    ~HeatAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    heat::ui::MainPanel& getMainPanel() noexcept { return panel; }

    static constexpr int designWidth = 1536;
    static constexpr int designHeight = 1024;

private:
    void applyScale (float scale);

    HeatAudioProcessor& processor;
    heat::ui::HeatLookAndFeel lookAndFeel;
    heat::ui::MainPanel panel;
    juce::TooltipWindow tooltips { this, 700 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::VBlankAttachment vblank;
    double lastFrameTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatAudioProcessorEditor)
};
