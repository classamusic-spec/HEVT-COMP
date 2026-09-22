#include "UI/HeatKnob.h"
#include "UI/HeatTheme.h"
#include "Graphics/SilverSurface.h"

namespace heat::ui
{
    namespace
    {
        layout::KnobGeometry geometryFor (HeatKnob::Style s)
        {
            switch (s)
            {
                case HeatKnob::Style::large:    return layout::largeKnob;
                case HeatKnob::Style::medium:   return layout::mediumKnob;
                case HeatKnob::Style::compress: return layout::compressKnob;
                case HeatKnob::Style::small:
                default:                        return layout::smallKnob;
            }
        }

        surface::SpunFaceStyle faceStyleFor (HeatKnob::Style s)
        {
            surface::SpunFaceStyle f;
            switch (s)
            {
                case HeatKnob::Style::large:    f = { 168.0f, 42.0f, 6.0f, 2.0f, 11 }; break;
                case HeatKnob::Style::compress: f = { 166.0f, 40.0f, 6.0f, 2.0f, 23 }; break;
                case HeatKnob::Style::medium:   f = { 160.0f, 30.0f, 10.0f, 2.0f, 37 }; break;
                case HeatKnob::Style::small:    f = { 158.0f, 26.0f, 12.0f, 2.0f, 41 }; break;
            }
            return f;
        }

        juce::Point<float> polar (juce::Point<float> c, float r, float angle)
        {
            return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
        }

        // Accessibility: exposes the knob as a ranged numeric value.
        class KnobAccessibility final : public juce::AccessibilityHandler
        {
        public:
            explicit KnobAccessibility (HeatKnob& k)
                : AccessibilityHandler (k, juce::AccessibilityRole::slider,
                                        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::showMenu,
                                                                                [&k] { k.showTextEntry(); }),
                                        Interfaces { std::make_unique<Value> (k) })
            {
            }

        private:
            class Value final : public juce::AccessibilityRangedNumericValueInterface
            {
            public:
                explicit Value (HeatKnob& k) : knob (k) {}
                bool isReadOnly() const override { return false; }
                double getCurrentValue() const override { return knob.getNormalisedValue(); }
                void setValue (double v) override { knob.setNormalisedFromUser ((float) v, true); }
                AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }

            private:
                HeatKnob& knob;
            };
        };
    }

    HeatKnob::HeatKnob (juce::RangedAudioParameter& parameter, juce::UndoManager* undoManager,
                        Style s, Arc a, juce::String accessibleName)
        : param (parameter),
          attachment (parameter, [this] (float v) { parameterChanged (v); }, undoManager),
          style (s), arc (a), name (std::move (accessibleName)), geometry (geometryFor (s))
    {
        const float extent = std::max ({ geometry.skirtRadius + geometry.shadowRadius + geometry.shadowOffset,
                                         geometry.arcRadius + 14.0f,
                                         geometry.trackOuter + 10.0f,
                                         geometry.tickRadius + 10.0f });
        margin = std::ceil (extent);
        setWantsKeyboardFocus (true);
        setTitle (name);
        setDescription (name + " control");
        setRepaintsOnMouseActivity (false);
        attachment.sendInitialUpdate();
    }

    HeatKnob::~HeatKnob() = default;

    void HeatKnob::setCentre (juce::Point<float> c)
    {
        setBounds (juce::Rectangle<float> (2.0f * margin, 2.0f * margin).withCentre (c).toNearestInt());
    }

    juce::Point<float> HeatKnob::centre() const
    {
        return { 0.5f * static_cast<float> (getWidth()), 0.5f * static_cast<float> (getHeight()) };
    }

    juce::Point<float> HeatKnob::getCentreInParent() const
    {
        return centre() + getPosition().toFloat();
    }

    void HeatKnob::setHeat (float h)
    {
        h = juce::jlimit (0.0f, 1.0f, h);
        if (std::abs (h - heat) > 0.002f)
        {
            heat = h;
            repaint();
        }
    }

    void HeatKnob::parameterChanged (float newValue)
    {
        const float v = param.convertTo0to1 (newValue);
        if (! juce::exactlyEqual (v, value))
        {
            value = v;
            repaint();
            if (auto* h = getAccessibilityHandler())
                h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
        }
    }

    juce::String HeatKnob::getValueText() const
    {
        return param.getText (value, 32);
    }

    juce::String HeatKnob::getTooltip()
    {
        auto t = name + "  " + getValueText();
        const auto help = juce::SettableTooltipClient::getTooltip();
        if (help.isNotEmpty())
            t << "\n" << help;
        if (extraInfo)
            t << "\n" << extraInfo();
        return t;
    }

    bool HeatKnob::hitTest (int x, int y)
    {
        const float reach = std::max (geometry.skirtRadius, geometry.trackOuter) + 4.0f;
        return centre().getDistanceFrom ({ static_cast<float> (x), static_cast<float> (y) }) <= reach;
    }

    float HeatKnob::angleFor (float v) const
    {
        return -rotaryHalfRange + 2.0f * rotaryHalfRange * v;
    }

    void HeatKnob::resized()
    {
        staticLayer = {};
    }

    // ------------------------------------------------------------------------------------
    // Painting
    // ------------------------------------------------------------------------------------

    void HeatKnob::renderStaticLayer (float pixelScale)
    {
        cachedScale = pixelScale;
        staticLayer = juce::Image (juce::Image::ARGB,
                                   std::max (1, juce::roundToInt (static_cast<float> (getWidth()) * pixelScale)),
                                   std::max (1, juce::roundToInt (static_cast<float> (getHeight()) * pixelScale)), true);
        juce::Graphics g (staticLayer);
        g.addTransform (juce::AffineTransform::scale (pixelScale));
        drawStaticParts (g);
    }

    void HeatKnob::drawStaticParts (juce::Graphics& g)
    {
        const auto c = centre();
        const auto& geo = geometry;
        const float R = geo.skirtRadius;

        // Soft contact shadow below the knob.
        {
            const juce::Point<float> sc (c.x, c.y + geo.shadowOffset);
            const float sr = R + geo.shadowRadius;
            const float strength = style == Style::small ? 0.55f : 0.40f;
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (strength), sc.x, sc.y,
                                         juce::Colours::black.withAlpha (0.0f), sc.x + sr, sc.y, true);
            shadow.addColour (R * 0.95f / sr, juce::Colours::black.withAlpha (strength * 0.9f));
            shadow.addColour ((R + geo.shadowRadius * 0.35f) / sr, juce::Colours::black.withAlpha (strength * 0.35f));
            g.setGradientFill (shadow);
            g.fillEllipse (sc.x - sr, sc.y - sr, 2 * sr, 2 * sr);
        }

        // Grey track ring (ATTACK / RELEASE).
        if (geo.trackOuter > 0.0f)
        {
            const float rMid = 0.5f * (geo.trackInner + geo.trackOuter);
            const float w = geo.trackOuter - geo.trackInner;
            juce::Path track;
            track.addCentredArc (c.x, c.y, rMid, rMid, 0.0f, -rotaryHalfRange, rotaryHalfRange, true);
            juce::ColourGradient tg (juce::Colour (0xffa2a5a8), c.x, c.y - geo.trackOuter,
                                     juce::Colour (0xff55595d), c.x, c.y + geo.trackOuter, false);
            tg.addColour (0.3, juce::Colour (0xff6e7175));
            g.setGradientFill (tg);
            g.strokePath (track, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            juce::Path inner;
            inner.addCentredArc (c.x, c.y, geo.trackInner, geo.trackInner, 0.0f, -rotaryHalfRange, rotaryHalfRange, true);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.strokePath (inner, juce::PathStrokeType (0.9f));
        }

        // Skirt: lit from above.
        {
            const bool small = style == Style::small;
            juce::ColourGradient sk (small ? juce::Colour (0xffd2d4d6) : juce::Colour (0xfff6f7f8), c.x, c.y - R,
                                     juce::Colour (0xff4c5054), c.x, c.y + R, false);
            sk.addColour (0.20, small ? juce::Colour (0xffbfc2c5) : juce::Colour (0xffd9dbdd));
            sk.addColour (0.50, juce::Colour (0xff888b8f));
            sk.addColour (0.76, juce::Colour (0xff595c61));
            sk.addColour (0.90, juce::Colour (0xff484b50));
            g.setGradientFill (sk);
            g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);

            g.setColour (juce::Colour (0xff2e3236).withAlpha (0.55f));
            g.drawEllipse (c.x - R + 0.5f, c.y - R + 0.5f, 2 * R - 1.0f, 2 * R - 1.0f, 1.0f);
        }

        // Dark base line where the skirt meets its seat.
        if (style == Style::large || style == Style::compress)
        {
            const float r = R - 4.0f;
            juce::ColourGradient dl (juce::Colour (0xff15181b).withAlpha (0.25f), c.x, c.y - r,
                                     juce::Colour (0xff15181b).withAlpha (0.85f), c.x, c.y + r, false);
            g.setGradientFill (dl);
            g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.6f);
        }
        else
        {
            const float r = R - 0.8f;
            juce::ColourGradient dl (juce::Colour (0xff15181b).withAlpha (0.35f), c.x, c.y - r,
                                     juce::Colour (0xff15181b).withAlpha (0.90f), c.x, c.y + r, false);
            g.setGradientFill (dl);
            g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.4f);
        }

        // Bevel ring around the face (raised face, lit from above).
        {
            const float r0 = geo.faceRadius + 0.5f;
            const float bw = std::max (2.0f, (R - geo.faceRadius) * 0.35f);
            const float r = r0 + 0.5f * bw;
            juce::ColourGradient bv (juce::Colours::white.withAlpha (0.25f), c.x, c.y - r,
                                     juce::Colours::black.withAlpha (0.35f), c.x, c.y + r, false);
            g.setGradientFill (bv);
            g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, bw);
        }

        // Spun-aluminium face.
        {
            const float fr = geo.faceRadius;
            const int px = std::max (8, juce::roundToInt (2.0f * fr * cachedScale));
            const auto face = surface::makeSpunFace (px, faceStyleFor (style));
            g.drawImage (face, juce::Rectangle<float> (c.x - fr, c.y - fr, 2 * fr, 2 * fr));

            juce::ColourGradient edge (juce::Colours::white.withAlpha (0.85f), c.x - fr, c.y - fr,
                                       juce::Colours::black.withAlpha (0.35f), c.x + fr, c.y + fr, false);
            edge.addColour (0.5, juce::Colours::white.withAlpha (0.15f));
            g.setGradientFill (edge);
            g.drawEllipse (c.x - fr, c.y - fr, 2 * fr, 2 * fr, 1.3f);
        }

        // COMPRESS: dark seat line for the heat ring, and the tick scale.
        if (style == Style::compress)
        {
            const float r = geo.arcRadius - 5.0f;
            g.setColour (juce::Colour (0xff15181b).withAlpha (0.75f));
            juce::Path seat;
            seat.addCentredArc (c.x, c.y, r, r, 0.0f, -rotaryHalfRange - 0.02f, rotaryHalfRange + 0.02f, true);
            g.strokePath (seat, juce::PathStrokeType (1.8f));

            const int ticks = 29;
            for (int i = 0; i < ticks; ++i)
            {
                const float t = static_cast<float> (i) / (ticks - 1);
                const float a = -rotaryHalfRange + 2.0f * rotaryHalfRange * t;
                const bool major = (i == 0 + 14) || i == 4 || i == 24;
                const float r0 = major ? geo.tickRadius - 1.5f : geo.tickRadius;
                const float r1 = major ? geo.tickRadius + 8.0f : geo.tickRadius + 5.5f;
                juce::Path tick;
                tick.startNewSubPath (polar (c, r0, a));
                tick.lineTo (polar (c, r1, a));
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.strokePath (tick, juce::PathStrokeType (major ? 2.3f : 1.8f), juce::AffineTransform::translation (0.0f, 0.8f));
                g.setColour (juce::Colour (0xff3d4146));
                g.strokePath (tick, juce::PathStrokeType (major ? 2.3f : 1.8f));
            }
        }

        // Small knobs: fine scale dots.
        if (style == Style::small && geo.tickRadius > 0.0f)
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = -rotaryHalfRange + 2.0f * rotaryHalfRange * static_cast<float> (i) / 10.0f;
                juce::Path tick;
                tick.startNewSubPath (polar (c, geo.tickRadius - (i == 5 ? 3.0f : 1.8f), a));
                tick.lineTo (polar (c, geo.tickRadius + (i == 5 ? 2.5f : 1.6f), a));
                g.setColour (juce::Colour (0xff4e5257).withAlpha (i == 5 ? 0.95f : 0.8f));
                g.strokePath (tick, juce::PathStrokeType (i == 5 ? 1.5f : 1.3f));
            }
        }
    }

    void HeatKnob::drawArc (juce::Graphics& g)
    {
        if (arc == Arc::none || geometry.arcRadius <= 0.0f)
            return;

        const auto c = centre();
        const float r = geometry.arcRadius;

        if (arc == Arc::amberHeat)
        {
            // The COMPRESS ring glows with compression activity.
            const float k = 0.55f + 0.45f * heat;
            juce::Path ring;
            ring.addCentredArc (c.x, c.y, r, r, 0.0f, -rotaryHalfRange, rotaryHalfRange, true);
            const juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::butt;
            g.setColour (juce::Colour (0xffffa060).withAlpha (0.08f * k));
            g.strokePath (ring, juce::PathStrokeType (30.0f, juce::PathStrokeType::curved, cap));
            g.setColour (juce::Colour (0xffffa060).withAlpha (0.14f * k));
            g.strokePath (ring, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, cap));
            g.setColour (juce::Colour (0xffff9a52).withAlpha (0.25f * k));
            g.strokePath (ring, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, cap));
            g.setColour (colours::amberDeep.withAlpha (0.85f * k));
            g.strokePath (ring, juce::PathStrokeType (5.2f, juce::PathStrokeType::curved, cap));
            g.setColour (colours::amber.withAlpha (k));
            g.strokePath (ring, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved, cap));
            g.setColour (colours::amberHot.withAlpha (0.9f * k));
            g.strokePath (ring, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, cap));
            return;
        }

        // Cyan value arc: bright at the anchored end, fading out towards the pointer.
        const float valueAngle = angleFor (value);
        const bool fromStart = arc == Arc::cyanFromStart;
        const float a0 = fromStart ? -rotaryHalfRange : rotaryHalfRange;
        const float span = valueAngle - a0;
        if (std::abs (span) < 0.01f)
            return;

        const float fade = style == Style::medium ? 0.50f : 0.25f;   // fraction of the arc that fades
        const float glowScale = style == Style::medium ? 0.65f : 1.0f;
        const int segments = std::max (6, (int) (std::abs (span) / 0.02f));
        for (int i = 0; i < segments; ++i)
        {
            const float t0 = static_cast<float> (i) / segments, t1 = static_cast<float> (i + 1) / segments;
            const float tm = 0.5f * (t0 + t1);
            float alpha = tm < 1.0f - fade ? 1.0f : juce::jmax (0.0f, (1.0f - tm) / fade);
            alpha = alpha * alpha * (3.0f - 2.0f * alpha);
            if (alpha <= 0.002f)
                continue;
            juce::Path seg;
            seg.addCentredArc (c.x, c.y, r, r, 0.0f, a0 + span * t0, a0 + span * t1 + (span > 0 ? 0.004f : -0.004f), true);
            g.setColour (juce::Colours::white.withAlpha (0.10f * alpha));
            g.strokePath (seg, juce::PathStrokeType (30.0f * glowScale));
            g.setColour (colours::cyanHot.withAlpha (0.20f * alpha));
            g.strokePath (seg, juce::PathStrokeType (16.0f * glowScale));
            g.setColour (colours::cyan.withAlpha (0.45f * alpha));
            g.strokePath (seg, juce::PathStrokeType (8.0f * glowScale));
            g.setColour (colours::cyan.withAlpha (0.95f * alpha));
            g.strokePath (seg, juce::PathStrokeType (4.4f * std::sqrt (glowScale)));
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.strokePath (seg, juce::PathStrokeType (2.2f * std::sqrt (glowScale)));
        }
    }

    void HeatKnob::drawPointer (juce::Graphics& g)
    {
        const auto c = centre();
        const float a = angleFor (value);
        const auto p0 = polar (c, geometry.pointerInner, a);
        const auto p1 = polar (c, geometry.pointerOuter, a);
        juce::Path line;
        line.startNewSubPath (p0);
        line.lineTo (p1);
        const juce::PathStrokeType stroke (geometry.pointerWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // Engraved: a faint highlight on the lower-right lip, then the dark line.
        const juce::Point<float> offset (0.55f * std::cos (a), 0.55f * std::sin (a));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (line, stroke, juce::AffineTransform::translation (offset.x + 0.3f, offset.y + 0.6f));
        g.setColour (colours::pointer);
        g.strokePath (line, stroke);
    }

    void HeatKnob::paint (juce::Graphics& g)
    {
        const float scale = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        if (staticLayer.isNull() || std::abs (cachedScale - scale) > 0.01f)
            renderStaticLayer (scale);

        g.drawImageTransformed (staticLayer, juce::AffineTransform::scale (1.0f / cachedScale));
        drawArc (g);
        drawPointer (g);

        if (hasKeyboardFocus (false))
        {
            const auto c = centre();
            const float r = geometry.skirtRadius + 3.0f;
            g.setColour (colours::cyan.withAlpha (0.7f));
            g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.2f);
        }
    }

    // ------------------------------------------------------------------------------------
    // Interaction
    // ------------------------------------------------------------------------------------

    bool HeatKnob::isFine (const juce::ModifierKeys& mods) const
    {
        return mods.isShiftDown() || mods.isCommandDown() || mods.isCtrlDown();
    }

    void HeatKnob::setNormalisedFromUser (float newValue, bool asCompleteGesture)
    {
        newValue = juce::jlimit (0.0f, 1.0f, newValue);
        const float plain = param.convertFrom0to1 (newValue);
        if (asCompleteGesture)
            attachment.setValueAsCompleteGesture (plain);
        else
            attachment.setValueAsPartOfGesture (plain);
    }

    void HeatKnob::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showContextMenu();
            return;
        }
        grabKeyboardFocus();
        dragging = true;
        dragFine = isFine (e.mods);
        dragAnchor = e.position;
        dragAnchorValue = value;
        attachment.beginGesture();
        if (onInteraction)
            onInteraction (*this, true);
    }

    void HeatKnob::mouseDrag (const juce::MouseEvent& e)
    {
        if (! dragging)
            return;

        const bool fine = isFine (e.mods);
        if (fine != dragFine)
        {
            // Re-anchor so switching precision never makes the value jump.
            dragFine = fine;
            dragAnchor = e.position;
            dragAnchorValue = value;
        }

        const float pixelsForFullRange = (style == Style::small ? 220.0f : style == Style::medium ? 260.0f : 320.0f)
                                         * (fine ? 8.0f : 1.0f);
        const float delta = (dragAnchor.y - e.position.y) + 0.35f * (e.position.x - dragAnchor.x);
        setNormalisedFromUser (dragAnchorValue + delta / pixelsForFullRange, false);
        if (onInteraction)
            onInteraction (*this, true);
    }

    void HeatKnob::mouseUp (const juce::MouseEvent&)
    {
        if (! dragging)
            return;
        dragging = false;
        attachment.endGesture();
        if (onInteraction)
            onInteraction (*this, isMouseOver());
    }

    void HeatKnob::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
            return;
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }

    void HeatKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        const float step = (isFine (e.mods) ? 0.002f : 0.02f) * (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX) * 5.0f;
        setNormalisedFromUser (value + (wheel.isReversed ? -step : step), true);
        if (onInteraction)
            onInteraction (*this, true);
    }

    void HeatKnob::mouseEnter (const juce::MouseEvent&)
    {
    }

    void HeatKnob::mouseExit (const juce::MouseEvent&)
    {
        if (onInteraction && ! dragging)
            onInteraction (*this, false);
    }

    bool HeatKnob::keyPressed (const juce::KeyPress& key)
    {
        const bool fine = key.getModifiers().isShiftDown();
        const float step = fine ? 0.002f : 0.01f;
        if (key.isKeyCode (juce::KeyPress::upKey) || key.isKeyCode (juce::KeyPress::rightKey))
            setNormalisedFromUser (value + step, true);
        else if (key.isKeyCode (juce::KeyPress::downKey) || key.isKeyCode (juce::KeyPress::leftKey))
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

    void HeatKnob::showContextMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (name + "  " + getValueText());
        menu.addItem (1, "Type Value...");
        menu.addItem (2, "Reset to Default");
        juce::Component::SafePointer<HeatKnob> safe (this);
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

    void HeatKnob::showTextEntry()
    {
        textEntry = std::make_unique<juce::TextEditor>();
        auto& te = *textEntry;
        te.setJustification (juce::Justification::centred);
        te.setFont (fontForCapHeight (Weight::medium, 11.0f));
        te.setColour (juce::TextEditor::backgroundColourId, colours::glassMid);
        te.setColour (juce::TextEditor::textColourId, colours::amberText);
        te.setColour (juce::TextEditor::outlineColourId, colours::amber);
        te.setColour (juce::TextEditor::focusedOutlineColourId, colours::amber);
        te.setText (getValueText(), false);
        te.selectAll();
        const auto c = centre();
        te.setBounds (juce::Rectangle<int> (120, 30).withCentre (c.toInt()));
        addAndMakeVisible (te);
        te.grabKeyboardFocus();

        juce::Component::SafePointer<HeatKnob> safe (this);
        auto commit = [safe]
        {
            if (safe == nullptr || safe->textEntry == nullptr)
                return;
            const auto text = safe->textEntry->getText();
            const float v = safe->param.getValueForText (text);
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

    std::unique_ptr<juce::AccessibilityHandler> HeatKnob::createAccessibilityHandler()
    {
        return std::make_unique<KnobAccessibility> (*this);
    }
}
