#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "UI/AdvancedPanel.h"
#include "UI/GpuMeter.h"
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
        AdvancedPanel& getAdvancedPanel() noexcept { return advanced; }
        GainReductionDisplay& getMeter() noexcept { return meter; }

        // GPU meter: the chassis leaves the meter glass transparent and the
        // meter component draws only its rim and scale overlay.
        void setGpuHole (bool enabled);
        bool hasGpuHole() const noexcept { return gpuHole; }

        std::function<void (float)> onUiScaleRequest;
        std::function<void (bool)> onGpuMeterRequest;

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
        bool gpuHole = false;
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

    // Attaches / detaches the OpenGL meter. Falls back to the CPU meter if the
    // context or shader cannot be created.
    void setGpuMeterEnabled (bool enabled);
    bool isGpuMeterActive() const noexcept { return gpuActive; }
    heat::ui::GpuMeterRenderer* getGpuRenderer() noexcept { return gpuRenderer.get(); }

    static constexpr int designWidth = 1536;
    static constexpr int designHeight = 1024;

private:
    void applyScale (float scale);
    void gpuReady (bool ok);

    HeatAudioProcessor& processor;
    heat::ui::HeatLookAndFeel lookAndFeel;
    heat::ui::MainPanel panel;
    juce::TooltipWindow tooltips { this, 700 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::VBlankAttachment vblank;
    double lastFrameTime = 0.0;

    juce::OpenGLContext glContext;
    std::unique_ptr<heat::ui::GpuMeterRenderer> gpuRenderer;
    bool gpuActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatAudioProcessorEditor)
};
