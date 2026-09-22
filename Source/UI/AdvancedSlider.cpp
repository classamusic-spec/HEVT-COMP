#include "UI/AdvancedSlider.h"
#include "UI/HeatTheme.h"

namespace heat::ui
{
    namespace
    {
        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }

        class SliderAccessibility final : public juce::AccessibilityHandler
        {
        public:
            explicit SliderAccessibility (AdvancedSlider& s)
                : AccessibilityHandler (s, juce::AccessibilityRole::slider,
                                        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::showMenu,
                                                                                [&s] { s.showTextEntry(); }),
                                        Interfaces { std::make_unique<Value> (s) })
            {
            }

        private:
            class Value final : public juce::AccessibilityRangedNumericValueInterface
            {
            public:
                explicit Value (AdvancedSlider& s) : slider (s) {}
                bool isReadOnly() const override { return false; }
                double getCurrentValue() const override { return slider.getNormalisedValue(); }
                void setValue (double v) override { slider.setNormalisedFromUser ((float) v, true); }
                AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }

            private:
                AdvancedSlider& slider;
            };
        };
    }

    AdvancedSlider::AdvancedSlider (juce::RangedAudioParameter& parameter, juce::UndoManager* undoManager, juce::String accessibleName)
        : param (parameter),
          attachment (parameter, [this] (float v)
                      {
                          value = param.convertTo0to1 (v);
                          repaint();
                          if (auto* h = getAccessibilityHandler())
                              h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
                      },
                      undoManager),
          name (std::move (accessibleName))
    {
        const auto range = param.getNormalisableRange();
        bipolar = range.start < 0.0f && std::abs (range.start + range.end) < 1.0e-4f;
        setWantsKeyboardFocus (true);
        setTitle (name);
        setDescription (name + " control");
        attachment.sendInitialUpdate();
    }

    AdvancedSlider::~AdvancedSlider() = default;

    void AdvancedSlider::setMeterDb (float db, float maxDb)
    {
        if (std::abs (db - meterDb) < 0.02f && std::abs (maxDb - meterMaxDb) < 0.01f)
            return;
        meterDb = db;
        meterMaxDb = maxDb;
        repaint();
    }

    void AdvancedSlider::setDimmed (bool shouldDim)
    {
        if (dimmed != shouldDim)
        {
            dimmed = shouldDim;
            repaint();
        }
    }

    juce::String AdvancedSlider::getValueText() const
    {
        return param.getCurrentValueAsText();
    }

    juce::Rectangle<float> AdvancedSlider::trackBounds() const
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromRight (valueWidth);
        return r.reduced (6.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 12.0f, 6.0f);
    }

    juce::Rectangle<float> AdvancedSlider::valueBounds() const
    {
        return getLocalBounds().toFloat().removeFromRight (valueWidth);
    }

    void AdvancedSlider::paint (juce::Graphics& g)
    {
        const auto track = trackBounds();
        const float alpha = dimmed ? 0.4f : 1.0f;

        // Glass groove.
        g.setColour (rgb (8, 9, 10, 0.9f * alpha));
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (rgb (255, 255, 255, 0.07f * alpha));
        g.drawRoundedRectangle (track.translated (0.0f, 0.5f), 3.0f, 1.0f);

        // Value fill (amber), from the start or from the centre.
        const float x = track.getX() + value * track.getWidth();
        const float from = bipolar ? track.getCentreX() : track.getX();
        auto fill = juce::Rectangle<float>::leftTopRightBottom (std::min (from, x), track.getY() + 1.0f,
                                                                std::max (from, x), track.getBottom() - 1.0f);
        if (fill.getWidth() > 0.5f)
        {
            juce::ColourGradient amber (colours::amber.withAlpha (0.55f * alpha), fill.getX(), 0.0f,
                                        colours::amberText.withAlpha (0.95f * alpha), fill.getRight(), 0.0f, false);
            if (bipolar && x < from)
                amber = juce::ColourGradient (colours::amberText.withAlpha (0.95f * alpha), fill.getX(), 0.0f,
                                              colours::amber.withAlpha (0.55f * alpha), fill.getRight(), 0.0f, false);
            g.setGradientFill (amber);
            g.fillRoundedRectangle (fill, 2.0f);
        }
        if (bipolar)
        {
            g.setColour (rgb (160, 164, 168, 0.6f * alpha));
            g.fillRect (juce::Rectangle<float> (track.getCentreX() - 0.5f, track.getY() - 3.0f, 1.0f, track.getHeight() + 6.0f));
        }

        // Silver thumb.
        const juce::Rectangle<float> thumb (x - 6.0f, track.getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f * alpha));
        g.fillEllipse (thumb.translated (0.0f, 1.2f));
        juce::ColourGradient silver (rgb (236, 238, 240, alpha), thumb.getX(), thumb.getY(),
                                     rgb (150, 154, 158, alpha), thumb.getX(), thumb.getBottom(), false);
        g.setGradientFill (silver);
        g.fillEllipse (thumb);
        g.setColour (rgb (60, 64, 68, 0.8f * alpha));
        g.drawEllipse (thumb, 0.8f);

        // Optional live reduction strip under the track.
        if (meterDb <= 0.0f && meterMaxDb > 0.0f)
        {
            const auto strip = juce::Rectangle<float> (track.getX(), track.getBottom() + 5.0f, track.getWidth(), 2.5f);
            g.setColour (rgb (30, 30, 32, 0.9f));
            g.fillRoundedRectangle (strip, 1.2f);
            const float amount = juce::jlimit (0.0f, 1.0f, -meterDb / meterMaxDb);
            if (amount > 0.001f)
            {
                g.setColour (rgb (255, 150, 70, 0.9f));
                g.fillRoundedRectangle (strip.withWidth (amount * strip.getWidth()), 1.2f);
            }
        }

        // Value text.
        const auto vb = valueBounds();
        const auto textColour = (hasKeyboardFocus (true) ? colours::amberText : rgb (214, 217, 220)).withMultipliedAlpha (alpha);
        drawText (g, getValueText().toUpperCase(), { Weight::medium, 8.0f, 0.8f, textColour },
                  vb.getRight() - 4.0f, vb.getCentreY() - 4.0f, juce::Justification::right);

        if (hasKeyboardFocus (false))
        {
            g.setColour (colours::cyan.withAlpha (0.6f));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
        }
    }

    bool AdvancedSlider::isFine (const juce::ModifierKeys& mods) const
    {
        return mods.isShiftDown() || mods.isCommandDown() || mods.isCtrlDown();
    }

    void AdvancedSlider::setNormalisedFromUser (float newValue, bool asCompleteGesture)
    {
        const float plain = param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, newValue));
        if (asCompleteGesture)
            attachment.setValueAsCompleteGesture (plain);
        else
            attachment.setValueAsPartOfGesture (plain);
    }

    void AdvancedSlider::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showContextMenu();
            return;
        }
        grabKeyboardFocus();
        if (valueBounds().contains (e.position))
        {
            showTextEntry();
            return;
        }
        dragging = true;
        dragFine = isFine (e.mods);
        attachment.beginGesture();
        const auto track = trackBounds();
        if (! dragFine && std::abs (e.position.x - (track.getX() + value * track.getWidth())) > 8.0f)
            setNormalisedFromUser ((e.position.x - track.getX()) / track.getWidth(), false); // jump to click
        dragAnchorX = e.position.x;
        dragAnchorValue = value;
    }

    void AdvancedSlider::mouseDrag (const juce::MouseEvent& e)
    {
        if (! dragging)
            return;
        const bool fine = isFine (e.mods);
        if (fine != dragFine)
        {
            dragFine = fine;
            dragAnchorX = e.position.x;
            dragAnchorValue = value;
        }
        const float width = trackBounds().getWidth() * (fine ? 8.0f : 1.0f);
        setNormalisedFromUser (dragAnchorValue + (e.position.x - dragAnchorX) / std::max (1.0f, width), false);
    }

    void AdvancedSlider::mouseUp (const juce::MouseEvent&)
    {
        if (! dragging)
            return;
        dragging = false;
        attachment.endGesture();
    }

    void AdvancedSlider::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() || valueBounds().contains (e.position))
            return;
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }

    void AdvancedSlider::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        const float d = std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX;
        const float step = (isFine (e.mods) ? 0.002f : 0.02f) * d * 5.0f;
        setNormalisedFromUser (value + (wheel.isReversed ? -step : step), true);
    }

    bool AdvancedSlider::keyPressed (const juce::KeyPress& key)
    {
        const float step = key.getModifiers().isShiftDown() ? 0.002f : 0.01f;
        if (key.isKeyCode (juce::KeyPress::rightKey) || key.isKeyCode (juce::KeyPress::upKey))
            setNormalisedFromUser (value + step, true);
        else if (key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::downKey))
            setNormalisedFromUser (value - step, true);
        else if (key.isKeyCode (juce::KeyPress::pageUpKey))
            setNormalisedFromUser (value + 0.1f, true);
        else if (key.isKeyCode (juce::KeyPress::pageDownKey))
            setNormalisedFromUser (value - 0.1f, true);
        else if (key.isKeyCode (juce::KeyPress::homeKey))
            setNormalisedFromUser (0.0f, true);
        else if (key.isKeyCode (juce::KeyPress::endKey))
            setNormalisedFromUser (1.0f, true);
        else if (key.isKeyCode (juce::KeyPress::returnKey))
            showTextEntry();
        else
            return false;
        return true;
    }

    void AdvancedSlider::showContextMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (name + "  " + getValueText());
        menu.addItem (1, "Type Value...");
        menu.addItem (2, "Reset to Default");
        juce::Component::SafePointer<AdvancedSlider> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1)
                                    safe->showTextEntry();
                                else if (result == 2)
                                    safe->attachment.setValueAsCompleteGesture (
                                        safe->param.convertFrom0to1 (safe->param.getDefaultValue()));
                            });
    }

    void AdvancedSlider::showTextEntry()
    {
        textEntry = std::make_unique<juce::TextEditor>();
        auto& te = *textEntry;
        te.setJustification (juce::Justification::centredRight);
        te.setFont (fontForCapHeight (Weight::medium, 8.0f));
        te.setColour (juce::TextEditor::backgroundColourId, colours::glassMid);
        te.setColour (juce::TextEditor::textColourId, colours::amberText);
        te.setColour (juce::TextEditor::outlineColourId, colours::amber);
        te.setColour (juce::TextEditor::focusedOutlineColourId, colours::amber);
        te.setText (getValueText(), false);
        te.selectAll();
        te.setBounds (valueBounds().toNearestInt());
        addAndMakeVisible (te);
        te.grabKeyboardFocus();

        juce::Component::SafePointer<AdvancedSlider> safe (this);
        auto commit = [safe]
        {
            if (safe == nullptr || safe->textEntry == nullptr)
                return;
            const float v = safe->param.getValueForText (safe->textEntry->getText());
            safe->attachment.setValueAsCompleteGesture (safe->param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)));
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->textEntry.reset(); });
        };
        te.onReturnKey = commit;
        te.onFocusLost = commit;
        te.onEscapeKey = [safe]
        {
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->textEntry.reset(); });
        };
    }

    std::unique_ptr<juce::AccessibilityHandler> AdvancedSlider::createAccessibilityHandler()
    {
        return std::make_unique<SliderAccessibility> (*this);
    }
}
