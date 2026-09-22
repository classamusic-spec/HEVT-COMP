#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace heat::ui
{
    // Row of pill buttons (MODE, DETECTOR and advanced-panel options).
    // Selection is never communicated by colour alone: the active pill is
    // dark with light text, inactive pills are silver with dark text, and
    // the active one also carries a bright outline.
    class SegmentedSelector : public juce::Component, public juce::TooltipClient
    {
    public:
        enum class Style { panel, glass };

        SegmentedSelector (juce::StringArray labels, Style style, juce::String accessibleName);
        ~SegmentedSelector() override;

        // Binds to a choice / bool parameter (index = plain value).
        void bindToParameter (juce::RangedAudioParameter& parameter, juce::UndoManager* undoManager);

        // Unbound use: callback on user selection.
        std::function<void (int)> onSelect;

        void setSelectedIndex (int index, bool notify);
        int getSelectedIndex() const noexcept { return selected; }
        int getNumItems() const noexcept { return labels.size(); }

        // Layout: pill width and gap (reference px); pills fill the height.
        void setPillGeometry (float pillWidth, float gap);

        void setTooltips (juce::StringArray tips) { tooltips = std::move (tips); }
        juce::String getTooltip() override;

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;
        void focusGained (FocusChangeType) override { repaint(); }
        void focusLost (FocusChangeType) override { repaint(); }

        juce::Rectangle<float> getPillBounds (int index) const;
        // Margin around the pill row reserved for glow / focus rings.
        static float insetFor (Style s) noexcept;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

        void choose (int index);

    private:
        int indexAt (juce::Point<float> p) const;
        void paintPanelPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool active, bool hover) const;
        void paintGlassPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool active, bool hover) const;

        juce::StringArray labels, tooltips;
        Style style;
        juce::String name;
        int selected = 0;
        int hovered = -1;
        float pillW = 0.0f, pillGap = 0.0f;
        float textTracking = -1.0f;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::RangedAudioParameter* param = nullptr;
    };
}
