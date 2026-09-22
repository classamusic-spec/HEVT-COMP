#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class HeatAudioProcessor;

namespace heat::ui
{
    // Button whose look is supplied by a paint callback.
    class PaintButton : public juce::Button
    {
    public:
        using PaintFn = std::function<void (juce::Graphics&, juce::Rectangle<float>, bool over, bool down)>;

        PaintButton (const juce::String& name, PaintFn fn) : juce::Button (name), painter (std::move (fn))
        {
            setWantsKeyboardFocus (true);
        }

        std::function<void()> onRightClick;

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            painter (g, getLocalBounds().toFloat(), over, down);
            if (hasKeyboardFocus (false))
            {
                g.setColour (juce::Colour (0xff8fd4ff).withAlpha (0.7f));
                g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.2f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onRightClick)
            {
                onRightClick();
                return;
            }
            juce::Button::mouseDown (e);
        }

    private:
        PaintFn painter;
    };

    // Preset display, settings gear and A/B comparison (header strip).
    class Header : public juce::Component, private juce::ChangeListener
    {
    public:
        explicit Header (HeatAudioProcessor& processor);
        ~Header() override;

        std::function<void()> onSettingsClicked;
        void setSettingsOpen (bool open);

        void refresh();
        void paint (juce::Graphics& g) override;
        void resized() override;

        void showPresetMenu();
        void showSavePresetDialog();

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        juce::Point<float> toLocal (float x, float y) const;
        juce::Rectangle<float> toLocal (juce::Rectangle<float> r) const;

        HeatAudioProcessor& processor;
        PaintButton prev, next, name, heart, gear, slotA, slotB;
        bool settingsOpen = false;
        juce::String shownName, shownTags;
    };
}
