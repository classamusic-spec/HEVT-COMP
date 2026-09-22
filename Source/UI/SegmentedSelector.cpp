#include "UI/SegmentedSelector.h"
#include "UI/HeatTheme.h"

namespace heat::ui
{
    namespace
    {
        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }

        // Tracking that the reference uses on pill captions ("CLEAN" spans 51.7 px at 10 px caps).
        float referencePillTracking()
        {
            static const float t = trackingForInkWidth ("CLEAN", Weight::medium, 10.0f, 51.7f);
            return t;
        }
    }

    SegmentedSelector::SegmentedSelector (juce::StringArray l, Style s, juce::String accessibleName)
        : labels (std::move (l)), style (s), name (std::move (accessibleName))
    {
        setWantsKeyboardFocus (true);
        setTitle (name);
        setDescription (name + " selector");
    }

    SegmentedSelector::~SegmentedSelector() = default;

    void SegmentedSelector::bindToParameter (juce::RangedAudioParameter& parameter, juce::UndoManager* undoManager)
    {
        param = &parameter;
        attachment = std::make_unique<juce::ParameterAttachment> (parameter, [this] (float v)
        {
            setSelectedIndex (juce::roundToInt (v), false);
        }, undoManager);
        attachment->sendInitialUpdate();
    }

    void SegmentedSelector::setPillGeometry (float pillWidth, float gap)
    {
        pillW = pillWidth;
        pillGap = gap;
        repaint();
    }

    void SegmentedSelector::setSelectedIndex (int index, bool notify)
    {
        index = juce::jlimit (0, labels.size() - 1, index);
        if (index == selected)
            return;
        selected = index;
        repaint();
        if (notify && onSelect)
            onSelect (selected);
        if (auto* h = getAccessibilityHandler())
            h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
    }

    void SegmentedSelector::choose (int index)
    {
        index = juce::jlimit (0, labels.size() - 1, index);
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture (static_cast<float> (index));
        setSelectedIndex (index, true);
    }

    float SegmentedSelector::insetFor (Style s) noexcept
    {
        return s == Style::panel ? 7.0f : 3.0f;
    }

    juce::Rectangle<float> SegmentedSelector::getPillBounds (int index) const
    {
        const int n = labels.size();
        const float inset = insetFor (style);
        const float h = static_cast<float> (getHeight()) - 2.0f * inset;
        const float w = pillW > 0.0f ? pillW
                                     : (static_cast<float> (getWidth()) - 2.0f * inset - pillGap * static_cast<float> (n - 1)) / static_cast<float> (n);
        return { inset + static_cast<float> (index) * (w + pillGap), inset, w, h };
    }

    int SegmentedSelector::indexAt (juce::Point<float> p) const
    {
        for (int i = 0; i < labels.size(); ++i)
            if (getPillBounds (i).contains (p))
                return i;
        return -1;
    }

    void SegmentedSelector::mouseDown (const juce::MouseEvent& e)
    {
        const int i = indexAt (e.position);
        if (i >= 0)
            choose (i);
    }

    void SegmentedSelector::mouseMove (const juce::MouseEvent& e)
    {
        const int i = indexAt (e.position);
        if (i != hovered)
        {
            hovered = i;
            repaint();
        }
    }

    juce::String SegmentedSelector::getTooltip()
    {
        if (hovered >= 0 && hovered < tooltips.size())
            return tooltips[hovered];
        return {};
    }

    void SegmentedSelector::mouseExit (const juce::MouseEvent&)
    {
        hovered = -1;
        repaint();
    }

    bool SegmentedSelector::keyPressed (const juce::KeyPress& key)
    {
        if (key.isKeyCode (juce::KeyPress::rightKey) || key.isKeyCode (juce::KeyPress::downKey))
            choose ((selected + 1) % labels.size());
        else if (key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::upKey))
            choose ((selected + labels.size() - 1) % labels.size());
        else
            return false;
        return true;
    }

    void SegmentedSelector::paintPanelPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text,
                                            bool active, bool hover) const
    {
        const float radius = 0.5f * r.getHeight();
        const auto inner = r.reduced (0.6f);

        if (active)
        {
            // Amber bloom on the panel.
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (rgb (255, 140, 70, 0.07f * static_cast<float> (4 - i)));
                g.fillRoundedRectangle (r.expanded (1.6f * static_cast<float> (i)), radius + 1.6f * static_cast<float> (i));
            }

            // Dark ember body, warmer towards its edges.
            juce::ColourGradient body (rgb (104, 69, 50), 0.0f, inner.getY(), rgb (112, 68, 47), 0.0f, inner.getBottom(), false);
            body.addColour (0.22, rgb (72, 51, 41));
            body.addColour (0.5, rgb (59, 44, 38));
            body.addColour (0.78, rgb (70, 51, 41));
            g.setGradientFill (body);
            g.fillRoundedRectangle (inner, radius);
            juce::ColourGradient side (rgb (130, 80, 50, 0.45f), inner.getX(), 0.0f, rgb (130, 80, 50, 0.45f), inner.getRight(), 0.0f, false);
            side.addColour (0.12, rgb (60, 44, 38, 0.0f));
            side.addColour (0.88, rgb (60, 44, 38, 0.0f));
            g.setGradientFill (side);
            g.fillRoundedRectangle (inner, radius);

            // Hot outline.
            g.setColour (rgb (255, 150, 80, 0.85f));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 3.2f);
            g.setColour (rgb (255, 238, 205));
            g.drawRoundedRectangle (r.reduced (1.2f), radius - 0.6f, 1.6f);

            TextSpec spec { Weight::semibold, 10.0f, referencePillTracking(), colours::amberText };
            drawText (g, text, spec, r.getCentreX(), r.getCentreY() - 5.0f, juce::Justification::centred);
            return;
        }

        // Soft drop shadow and silver body.
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f).expanded (0.5f), radius + 0.5f);
        juce::ColourGradient body (rgb (182, 184, 186), 0.0f, inner.getY(), rgb (171, 173, 177), 0.0f, inner.getBottom(), false);
        if (hover)
            body = juce::ColourGradient (rgb (192, 194, 196), 0.0f, inner.getY(), rgb (180, 182, 186), 0.0f, inner.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (inner, radius);

        // Outline with an inner top highlight and an outer bottom highlight.
        g.setColour (rgb (92, 95, 98));
        g.drawRoundedRectangle (r.reduced (0.6f), radius, 1.3f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.withHeight (r.getHeight() * 0.5f).toNearestInt());
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (1.9f), radius - 1.3f, 1.0f);
        }
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.expanded (2.0f).withTrimmedTop (r.getHeight() * 0.55f).toNearestInt());
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawRoundedRectangle (r.expanded (0.7f), radius + 0.7f, 1.0f);
        }

        TextSpec spec { Weight::medium, 10.0f, referencePillTracking(), rgb (48, 52, 56) };
        drawText (g, text, spec, r.getCentreX(), r.getCentreY() - 5.0f, juce::Justification::centred);
    }

    void SegmentedSelector::paintGlassPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text,
                                            bool active, bool hover) const
    {
        const float radius = 0.5f * r.getHeight();
        if (active)
        {
            g.setColour (rgb (255, 140, 70, 0.14f));
            g.fillRoundedRectangle (r.expanded (2.0f), radius + 2.0f);
            g.setColour (rgb (58, 38, 28));
            g.fillRoundedRectangle (r.reduced (0.5f), radius);
            g.setColour (rgb (255, 170, 105));
            g.drawRoundedRectangle (r.reduced (0.8f), radius - 0.3f, 1.4f);
        }
        else
        {
            g.setColour (hover ? rgb (52, 55, 59) : rgb (38, 41, 44));
            g.fillRoundedRectangle (r.reduced (0.5f), radius);
            g.setColour (rgb (90, 94, 99));
            g.drawRoundedRectangle (r.reduced (0.8f), radius - 0.3f, 1.0f);
        }
        TextSpec spec { active ? Weight::semibold : Weight::medium, 8.0f, 1.6f,
                        active ? colours::amberText : rgb (178, 183, 188) };
        drawText (g, text, spec, r.getCentreX(), r.getCentreY() - 4.0f, juce::Justification::centred);
    }

    void SegmentedSelector::paint (juce::Graphics& g)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            const auto r = getPillBounds (i);
            if (style == Style::panel)
                paintPanelPill (g, r, labels[i], i == selected, i == hovered);
            else
                paintGlassPill (g, r, labels[i], i == selected, i == hovered);
        }

        if (hasKeyboardFocus (false))
        {
            const auto r = getPillBounds (selected).expanded (3.0f);
            g.setColour (colours::cyan.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 0.5f * r.getHeight(), 1.2f);
        }
    }

    std::unique_ptr<juce::AccessibilityHandler> SegmentedSelector::createAccessibilityHandler()
    {
        class Handler final : public juce::AccessibilityHandler
        {
        public:
            explicit Handler (SegmentedSelector& s)
                : AccessibilityHandler (s, juce::AccessibilityRole::group, {},
                                        Interfaces { std::make_unique<Value> (s) }) {}

        private:
            class Value final : public juce::AccessibilityTextValueInterface
            {
            public:
                explicit Value (SegmentedSelector& s) : sel (s) {}
                bool isReadOnly() const override { return false; }
                juce::String getCurrentValueAsString() const override { return sel.labels[sel.getSelectedIndex()]; }
                void setValueAsString (const juce::String& v) override
                {
                    const int i = sel.labels.indexOf (v, true);
                    if (i >= 0)
                        sel.choose (i);
                }

            private:
                SegmentedSelector& sel;
            };
        };
        return std::make_unique<Handler> (*this);
    }
}
