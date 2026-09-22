#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "UI/Layout.h"

namespace heat::ui
{
    // Machined silver rotary control bound to one parameter.
    //
    //   drag vertically (or horizontally)    coarse
    //   Shift / Cmd / Ctrl + drag            fine (8x slower)
    //   double-click                         reset to default
    //   mouse wheel                          step
    //   right-click                          type value / reset menu
    //   arrow keys / Page / Home / End       keyboard control (Shift = fine)
    class HeatKnob : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum class Style { large, medium, compress, small };
        enum class Arc { none, cyanFromStart, cyanFromEnd, amberHeat };

        HeatKnob (juce::RangedAudioParameter& parameter, juce::UndoManager* undoManager,
                  Style style, Arc arc, juce::String accessibleName);
        ~HeatKnob() override;

        // Places the knob so its rotation centre lands on `centre` (parent coordinates).
        void setCentre (juce::Point<float> centre);

        // COMPRESS: 0..1 glow intensity of the amber ring (driven by gain reduction).
        void setHeat (float heat01);

        // Called when the user starts / stops interacting (value bubble).
        std::function<void (HeatKnob&, bool active)> onInteraction;
        // Optional extra line for the value bubble / tooltip (COMPRESS details).
        std::function<juce::String()> extraInfo;

        juce::String getValueText() const;
        juce::String getDisplayName() const { return name; }
        float getNormalisedValue() const noexcept { return value; }
        juce::RangedAudioParameter& getParameter() noexcept { return param; }
        juce::Point<float> getCentreInParent() const;
        float getFaceRadius() const noexcept { return geometry.faceRadius; }

        void paint (juce::Graphics& g) override;
        void resized() override;
        bool hitTest (int x, int y) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseEnter (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;
        void focusGained (FocusChangeType) override { repaint(); }
        void focusLost (FocusChangeType) override { repaint(); }
        juce::String getTooltip() override;

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

        void setNormalisedFromUser (float newValue, bool asCompleteGesture);
        void showTextEntry();

    private:
        void parameterChanged (float newValue);
        void renderStaticLayer (float pixelScale);
        void drawStaticParts (juce::Graphics& g);
        void drawArc (juce::Graphics& g);
        void drawPointer (juce::Graphics& g);
        juce::Point<float> centre() const;
        float angleFor (float v) const;
        void showContextMenu();
        bool isFine (const juce::ModifierKeys& mods) const;

        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;
        Style style;
        Arc arc;
        juce::String name;
        layout::KnobGeometry geometry;
        float margin = 0.0f;

        float value = 0.0f;
        float heat = 0.0f;

        juce::Image staticLayer;
        float cachedScale = 0.0f;

        // Drag state.
        bool dragging = false;
        bool dragFine = false;
        float dragAnchorValue = 0.0f;
        juce::Point<float> dragAnchor;

        std::unique_ptr<juce::TextEditor> textEntry;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatKnob)
    };
}
