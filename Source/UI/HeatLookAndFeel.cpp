#include "UI/HeatLookAndFeel.h"
#include "UI/HeatTheme.h"

namespace heat::ui
{
    namespace
    {
        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }
    }

    HeatLookAndFeel::HeatLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, rgb (24, 26, 28));
        setColour (juce::PopupMenu::textColourId, rgb (214, 218, 222));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, rgb (70, 44, 30));
        setColour (juce::PopupMenu::highlightedTextColourId, colours::amberText);
        setColour (juce::TooltipWindow::backgroundColourId, rgb (22, 24, 26, 0.96f));
        setColour (juce::TooltipWindow::textColourId, rgb (220, 224, 228));
        setColour (juce::TooltipWindow::outlineColourId, rgb (120, 124, 128));
        setColour (juce::AlertWindow::backgroundColourId, rgb (26, 28, 30));
        setColour (juce::AlertWindow::textColourId, rgb (220, 224, 228));
        setColour (juce::AlertWindow::outlineColourId, rgb (140, 144, 148));
        setColour (juce::TextEditor::backgroundColourId, rgb (14, 15, 16));
        setColour (juce::TextEditor::textColourId, colours::amberText);
        setColour (juce::TextEditor::outlineColourId, rgb (90, 94, 98));
        setColour (juce::TextEditor::focusedOutlineColourId, colours::amber);
        setColour (juce::TextEditor::highlightColourId, colours::amber.withAlpha (0.35f));
        setColour (juce::CaretComponent::caretColourId, colours::amber);
        setColour (juce::TextButton::buttonColourId, rgb (44, 47, 50));
        setColour (juce::TextButton::buttonOnColourId, rgb (70, 44, 30));
        setColour (juce::TextButton::textColourOffId, rgb (214, 218, 222));
        setColour (juce::TextButton::textColourOnId, colours::amberText);
        setColour (juce::ResizableWindow::backgroundColourId, rgb (26, 28, 30));
    }

    juce::Typeface::Ptr HeatLookAndFeel::getTypefaceForFont (const juce::Font& f)
    {
        return typeface (f.isBold() ? Weight::semibold : Weight::regular);
    }

    juce::Font HeatLookAndFeel::getPopupMenuFont()
    {
        return fontForCapHeight (Weight::regular, 9.5f);
    }

    void HeatLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
        juce::ColourGradient bg (rgb (32, 35, 38), 0.0f, 0.0f, rgb (20, 22, 24), 0.0f, r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRect (r);
        g.setColour (rgb (150, 154, 158, 0.5f));
        g.drawRect (r, 1.0f);
    }

    void HeatLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                             bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                             const juce::String& text, const juce::String& shortcutKeyText,
                                             const juce::Drawable*, const juce::Colour*)
    {
        auto r = area.toFloat();
        if (isSeparator)
        {
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (r.withSizeKeepingCentre (r.getWidth() - 16.0f, 1.0f));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour (rgb (70, 44, 30));
            g.fillRoundedRectangle (r.reduced (3.0f, 1.0f), 6.0f);
            g.setColour (colours::amber.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (3.0f, 1.0f), 6.0f, 1.0f);
        }

        const auto colour = ! isActive ? rgb (110, 114, 118)
                                       : isHighlighted ? colours::amberText : rgb (214, 218, 222);
        if (isTicked)
        {
            g.setColour (colours::amber);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 14.0f, r.getCentreY() }));
        }

        TextSpec spec { Weight::regular, 9.0f, 0.6f, colour };
        drawText (g, text, spec, r.getX() + 26.0f, r.getCentreY() - 4.5f, juce::Justification::left);

        if (hasSubMenu)
        {
            juce::Path arrow;
            const float x = r.getRight() - 14.0f, y = r.getCentreY();
            arrow.startNewSubPath (x - 3.0f, y - 4.0f);
            arrow.lineTo (x + 1.0f, y);
            arrow.lineTo (x - 3.0f, y + 4.0f);
            g.setColour (colour);
            g.strokePath (arrow, juce::PathStrokeType (1.4f));
        }
        else if (shortcutKeyText.isNotEmpty())
        {
            drawText (g, shortcutKeyText, { Weight::regular, 8.0f, 0.4f, rgb (130, 134, 138) },
                      r.getRight() - 12.0f, r.getCentreY() - 4.0f, juce::Justification::right);
        }
    }

    void HeatLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName)
    {
        drawText (g, sectionName, { Weight::medium, 8.0f, 1.8f, rgb (160, 164, 168) },
                  static_cast<float> (area.getX()) + 12.0f, static_cast<float> (area.getCentreY()) - 4.0f,
                  juce::Justification::left);
    }

    juce::Rectangle<int> HeatLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                            juce::Rectangle<int> parentArea)
    {
        const auto lines = juce::StringArray::fromLines (tipText);
        float w = 0.0f;
        for (const auto& l : lines)
            w = std::max (w, textPath (l, { Weight::regular, 8.5f, 0.5f, {} }).getBounds().getWidth());
        const int width = juce::roundToInt (w) + 28;
        const int height = 14 + 17 * lines.size();
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (width + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (height + 6) : screenPos.y + 8,
                                     width, height).constrainedWithin (parentArea);
    }

    void HeatLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
        g.setColour (rgb (22, 24, 26, 0.96f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (rgb (120, 124, 128, 0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        const auto lines = juce::StringArray::fromLines (text);
        for (int i = 0; i < lines.size(); ++i)
            drawText (g, lines[i], { i == 0 ? Weight::medium : Weight::regular, 8.5f, 0.5f,
                                     i == 0 ? rgb (226, 230, 234) : rgb (170, 175, 180) },
                      14.0f, 10.0f + 17.0f * static_cast<float> (i), juce::Justification::left);
    }

    juce::Font HeatLookAndFeel::getAlertWindowTitleFont() { return fontForCapHeight (Weight::medium, 11.0f); }
    juce::Font HeatLookAndFeel::getAlertWindowMessageFont() { return fontForCapHeight (Weight::regular, 9.5f); }
    juce::Font HeatLookAndFeel::getAlertWindowFont() { return fontForCapHeight (Weight::regular, 9.5f); }
    juce::Font HeatLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return fontForCapHeight (Weight::medium, 9.0f); }

    void HeatLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
    {
        // Three subtle engraved diagonal strokes.
        const float alpha = isMouseDragging ? 0.8f : isMouseOver ? 0.6f : 0.35f;
        for (int i = 1; i <= 3; ++i)
        {
            const float o = static_cast<float> (i) * static_cast<float> (std::min (w, h)) / 4.0f;
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawLine (static_cast<float> (w) - o + 1.0f, static_cast<float> (h), static_cast<float> (w), static_cast<float> (h) - o + 1.0f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (alpha * 0.8f));
            g.drawLine (static_cast<float> (w) - o, static_cast<float> (h), static_cast<float> (w), static_cast<float> (h) - o, 1.0f);
        }
    }
}
