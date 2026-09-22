#pragma once

#include "UI/AdvancedSlider.h"
#include "UI/SegmentedSelector.h"

class HeatAudioProcessor;

namespace heat::ui
{
    // Settings popover opened from the header gear. Everything beyond the
    // front panel lives here, on four pages:
    //
    //   GENERAL    stereo mode, link, look-ahead, auto makeup / release,
    //              quality, IRON model
    //   SIDECHAIN  source, listen, HPF, LPF, EQ frequency / gain / Q
    //   MULTIBAND  mode, crossovers, band amounts (with live band GR)
    //   OUTPUT     limiter, ceiling (with live limiter GR), meter hold,
    //              GPU meter, UI size
    //
    // The footer shows the current latency. Nothing here is needed for the
    // main workflow; the front panel stays exactly the locked reference.
    class AdvancedPanel : public juce::Component
    {
    public:
        enum class Page { general = 0, sidechain, multiband, output };

        explicit AdvancedPanel (HeatAudioProcessor& processor);
        ~AdvancedPanel() override;

        std::function<void()> onClose;
        std::function<void (float)> onUiScale;   // 0.75 / 1.0 / 1.25
        std::function<void (bool)> onPeakHold;
        std::function<void (bool)> onGpuMeter;

        void setPage (Page page);
        Page getPage() const noexcept { return page; }

        void syncFromProcessor();
        // Called at frame rate while the panel is open (live GR strips, latency).
        void updateLive (double elapsedSeconds);

        void paint (juce::Graphics& g) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress& key) override;

        static juce::Rectangle<float> panelBounds();
        static constexpr int maxRowsPerPage = 7;

    private:
        struct Row
        {
            Page page;
            juce::String label;
            std::unique_ptr<SegmentedSelector> selector;
            std::unique_ptr<AdvancedSlider> slider;
            juce::Component* control() const { return selector != nullptr ? static_cast<juce::Component*> (selector.get()) : slider.get(); }
        };

        SegmentedSelector* addChoice (Page page, const juce::String& label, juce::StringArray options,
                                      juce::StringArray tips, const char* paramId);
        AdvancedSlider* addSlider (Page page, const juce::String& label, const char* paramId, const juce::String& tip);
        void updateDimming();

        HeatAudioProcessor& processor;
        SegmentedSelector tabs;
        std::vector<Row> rows;
        Page page = Page::general;

        SegmentedSelector* meterHold = nullptr;
        SegmentedSelector* uiSize = nullptr;
        SegmentedSelector* gpuMeter = nullptr;
        AdvancedSlider* bandSliders[3] { nullptr, nullptr, nullptr };
        AdvancedSlider* xoverHigh = nullptr;
        AdvancedSlider* ceiling = nullptr;

        float bandShown[3] { 0.0f, 0.0f, 0.0f };
        float limiterShown = 0.0f;
        int shownLatency = -1;
    };
}
