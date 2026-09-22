#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace heat::ui
{
    // Slim horizontal glass slider for the advanced panel, bound to one
    // parameter. Same interaction model as the front-panel knobs:
    //
    //   drag (horizontal)            coarse; Shift / Cmd / Ctrl = 8x fine
    //   double-click                 reset to default
    //   mouse wheel                  step
    //   click on the value / Return  type a value
    //   right-click                  type value / reset menu
    //   arrow keys / Page / Home / End
    //
    // Bipolar parameters (range symmetric around 0) fill from the centre.
    // An optional meter strip (e.g. band gain reduction) can be shown under
    // the track.
    class AdvancedSlider : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        AdvancedSlider (juce::RangedAudioParameter& parameter, juce::UndoManager* undoManager, juce::String accessibleName);
        ~AdvancedSlider() override;

        // Live reduction strip (0 … -maxDb) drawn under the track; db > 0 hides it.
        void setMeterDb (float db, float maxDb = 12.0f);
        void setDimmed (bool shouldDim);

        juce::String getValueText() const;
        float getNormalisedValue() const noexcept { return value; }
        void setNormalisedFromUser (float newValue, bool asCompleteGesture);
        void showTextEntry();

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        bool keyPressed (const juce::KeyPress& key) override;
        void focusGained (FocusChangeType) override { repaint(); }
        void focusLost (FocusChangeType) override { repaint(); }
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

        static constexpr float valueWidth = 78.0f;

    private:
        juce::Rectangle<float> trackBounds() const;
        juce::Rectangle<float> valueBounds() const;
        bool isFine (const juce::ModifierKeys& mods) const;
        void showContextMenu();

        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;
        juce::String name;
        float value = 0.0f;
        bool bipolar = false;
        bool dimmed = false;
        float meterDb = 1.0f, meterMaxDb = 12.0f;   // > 0 = strip hidden

        bool dragging = false, dragFine = false;
        float dragAnchorValue = 0.0f, dragAnchorX = 0.0f;
        std::unique_ptr<juce::TextEditor> textEntry;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedSlider)
    };
}
