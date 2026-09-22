#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace heat::ui
{
    // Styles the few stock JUCE elements HEAT still uses (popup menus,
    // tooltips, alert windows, text entry, resize corner) so nothing looks
    // like a default JUCE plugin.
    class HeatLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        HeatLookAndFeel();

        juce::Font getPopupMenuFont() override;
        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                const juce::Colour* textColour) override;
        void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
        int getPopupMenuBorderSize() override { return 6; }

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

        juce::Font getAlertWindowTitleFont() override;
        juce::Font getAlertWindowMessageFont() override;
        juce::Font getAlertWindowFont() override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

        void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    };
}
