#pragma once

#include "UI/SegmentedSelector.h"

class HeatAudioProcessor;

namespace heat::ui
{
    // Compact settings popover opened from the header gear. Holds only
    // session-level options; nothing here is needed for the main workflow.
    class AdvancedPanel : public juce::Component
    {
    public:
        explicit AdvancedPanel (HeatAudioProcessor& processor);

        std::function<void()> onClose;
        std::function<void (float)> onUiScale;   // 0.75 / 1.0 / 1.25
        std::function<void (bool)> onPeakHold;

        void syncFromProcessor();
        void paint (juce::Graphics& g) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress& key) override;

        static juce::Rectangle<float> panelBounds();

    private:
        struct Row
        {
            juce::String label;
            std::unique_ptr<SegmentedSelector> selector;
        };

        void addRow (const juce::String& label, juce::StringArray options, juce::StringArray tips,
                     const char* paramId);

        HeatAudioProcessor& processor;
        std::vector<Row> rows;
        SegmentedSelector* meterHold = nullptr;
        SegmentedSelector* uiSize = nullptr;
    };
}
