#include "UI/GainReductionDisplay.h"
#include "UI/HeatTheme.h"
#include "UI/Layout.h"

namespace heat::ui
{
    using namespace heat::ui::layout;

    namespace
    {
        constexpr float margin = 12.0f;
        constexpr float releaseTau = 0.060f;
        constexpr double peakHoldSeconds = 1.5;
        constexpr float peakFallDbPerSecond = 15.0f;

        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }

    }

    // How "hot" the meter burns for a given reduction: dark at rest, gentle
    // for light glue, fully lit from 3 dB.
    float GainReductionDisplay::heatFor (float db)
    {
        const float x = juce::jlimit (0.0f, 1.0f, -db / 3.0f);
        return x * x * (3.0f - 2.0f * x);
    }

    juce::Rectangle<float> GainReductionDisplay::glassBounds()
    {
        return meterOuter.reduced (meterRim);
    }

    float GainReductionDisplay::glassRadius()
    {
        return meterOuterRadius - meterRim;
    }

    float GainReductionDisplay::yForDb (float db)
    {
        db = juce::jlimit (-30.0f, 0.0f, db);
        for (int i = 0; i < 4; ++i)
            if (db >= meterDb[i + 1])
            {
                const float t = (db - meterDb[i]) / (meterDb[i + 1] - meterDb[i]);
                return meterDbY[i] + t * (meterDbY[i + 1] - meterDbY[i]);
            }
        // Below -24 dB: continue at the -12..-24 spacing, clamped to the column base.
        const float t = (db - meterDb[4]) / (meterDb[4] - meterDb[3]);
        return std::min (meterColumnBottom - 4.0f, meterDbY[4] + t * (meterDbY[4] - meterDbY[3]));
    }

    GainReductionDisplay::GainReductionDisplay()
    {
        setInterceptsMouseClicks (false, false);
        setTitle ("Gain Reduction");
        setDescription ("Gain reduction meter");
    }

    void GainReductionDisplay::setInstrumentBounds (juce::Rectangle<float> outer)
    {
        outerBounds = outer;
        setBounds (outer.expanded (margin).toNearestInt());
    }

    void GainReductionDisplay::resized()
    {
        background = {};
        overlay = {};
    }

    void GainReductionDisplay::setPeakHoldEnabled (bool enabled)
    {
        peakHold = enabled;
        repaint();
    }

    void GainReductionDisplay::setGpuMode (bool enabled)
    {
        if (gpuMode == enabled)
            return;
        gpuMode = enabled;
        background = {};
        overlay = {};
        repaint();
        if (gpuMode && onGpuFrame)
            onGpuFrame();
    }

    void GainReductionDisplay::setImmediate (float leftDb, float rightDb, float peakDb)
    {
        shown[0] = std::min (0.0f, leftDb);
        shown[1] = std::min (0.0f, rightDb);
        peak = std::min ({ 0.0f, peakDb, shown[0], shown[1] });
        peakHoldTime = 0.0;
        if (gpuMode && onGpuFrame)
            onGpuFrame();
        else
            repaint();
    }

    void GainReductionDisplay::pushFrame (float deepestLeftDb, float deepestRightDb, double dt)
    {
        const float measured[2] { std::min (0.0f, deepestLeftDb), std::min (0.0f, deepestRightDb) };
        const float k = 1.0f - std::exp (-static_cast<float> (dt) / releaseTau);
        bool changed = false;

        for (int ch = 0; ch < 2; ++ch)
        {
            const float before = shown[ch];
            if (measured[ch] < shown[ch])
                shown[ch] = measured[ch];                     // attack: immediate
            else
                shown[ch] += k * (measured[ch] - shown[ch]);  // release: weighted
            if (shown[ch] > -0.01f)
                shown[ch] = 0.0f;
            changed = changed || std::abs (before - shown[ch]) > 0.005f;
        }

        const float deepest = std::min (shown[0], shown[1]);
        if (deepest <= peak)
        {
            peak = deepest;
            peakHoldTime = 0.0;
        }
        else
        {
            peakHoldTime += dt;
            if (! peakHold || peakHoldTime > peakHoldSeconds)
            {
                const float before = peak;
                peak = peakHold ? std::min (deepest, peak + peakFallDbPerSecond * static_cast<float> (dt)) : deepest;
                changed = changed || std::abs (before - peak) > 0.005f;
            }
        }

        if (changed)
        {
            if (gpuMode && onGpuFrame)
                onGpuFrame();
            else
                repaint();
            if (auto* h = getAccessibilityHandler())
                h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
        }
    }

    // --------------------------------------------------------------------------------
    void GainReductionDisplay::renderLayers (float pixelScale)
    {
        cachedScale = pixelScale;
        const int w = std::max (1, juce::roundToInt (static_cast<float> (getWidth()) * pixelScale));
        const int h = std::max (1, juce::roundToInt (static_cast<float> (getHeight()) * pixelScale));
        const auto toRef = juce::AffineTransform::translation (-static_cast<float> (getX()), -static_cast<float> (getY()))
                               .scaled (pixelScale);

        background = juce::Image (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (background);
            g.addTransform (toRef);
            if (gpuMode)
            {
                // Leave the glass interior clear: the GPU layer shows through.
                juce::Path hole;
                hole.addRectangle (meterOuter.expanded (margin));
                hole.addRoundedRectangle (glassBounds(), glassRadius());
                hole.setUsingNonZeroWinding (false);
                g.reduceClipRegion (hole);
            }
            paintBackground (g);
        }
        overlay = juce::Image (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (overlay);
            g.addTransform (toRef);
            paintOverlay (g);
        }
    }

    void GainReductionDisplay::paintBackground (juce::Graphics& g)
    {
        const auto outer = meterOuter;
        const auto glass = outer.reduced (meterRim);

        // Soft outer shadow on the panel.
        {
            juce::Path p;
            p.addRoundedRectangle (outer.translated (0.0f, 4.0f), meterOuterRadius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.28f), 12, { 0, 3 }).drawForPath (g, p);
        }

        // Machined silver rim.
        {
            juce::Path rim;
            rim.addRoundedRectangle (outer, meterOuterRadius);
            juce::ColourGradient rg (rgb (228, 230, 232), outer.getX(), outer.getY(),
                                     rgb (196, 190, 186), outer.getX(), outer.getBottom(), false);
            rg.addColour (0.5, rgb (206, 206, 207));
            g.setGradientFill (rg);
            g.fillPath (rim);
            g.setColour (rgb (80, 84, 88, 0.85f));
            g.strokePath (rim, juce::PathStrokeType (1.0f));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            juce::Path hi;
            hi.addRoundedRectangle (outer.reduced (1.2f), meterOuterRadius - 1.2f);
            g.strokePath (hi, juce::PathStrokeType (1.0f));
        }

        // Dark glass body.
        juce::Path glassPath;
        glassPath.addRoundedRectangle (glass, meterOuterRadius - meterRim);
        {
            juce::ColourGradient gg (rgb (8, 8, 9), glass.getX(), glass.getY(),
                                     rgb (24, 20, 18), glass.getX(), glass.getBottom(), false);
            gg.addColour (0.35, rgb (12, 12, 13));
            g.setGradientFill (gg);
            g.fillPath (glassPath);
            // Inner shadow along the top edge.
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (glassPath);
            juce::ColourGradient top (juce::Colours::black.withAlpha (0.7f), glass.getX(), glass.getY(),
                                      juce::Colours::transparentBlack, glass.getX(), glass.getY() + 26.0f, false);
            g.setGradientFill (top);
            g.fillRect (glass);
        }

        // Thin warm inner rim (the reference glass carries an amber edge).
        {
            juce::Path edge;
            edge.addRoundedRectangle (glass.reduced (0.6f), meterOuterRadius - meterRim - 0.6f);
            juce::ColourGradient eg (rgb (255, 160, 95, 0.55f), 0.0f, glass.getY(), rgb (255, 200, 150, 0.95f), 0.0f, glass.getBottom(), false);
            eg.addColour (0.5, rgb (240, 130, 70, 0.6f));
            g.setGradientFill (eg);
            g.strokePath (edge, juce::PathStrokeType (2.2f));
            juce::Path inner;
            inner.addRoundedRectangle (glass.reduced (3.0f), meterOuterRadius - meterRim - 3.0f);
            juce::ColourGradient ig (rgb (255, 130, 60, 0.10f), 0.0f, glass.getY(), rgb (255, 130, 60, 0.32f), 0.0f, glass.getBottom(), false);
            g.setGradientFill (ig);
            g.strokePath (inner, juce::PathStrokeType (4.0f));
        }

        // Empty glass columns (the lit bodies are drawn on top per frame).
        auto column = [&g] (float x0, float x1)
        {
            juce::Path p;
            p.addRoundedRectangle (x0, meterColumnTop, x1 - x0, meterColumnBottom - meterColumnTop, 10.0f);
            juce::ColourGradient cg (rgb (24, 24, 25, 0.0f), 0.0f, meterColumnTop - 20.0f,
                                     rgb (30, 27, 25), 0.0f, meterColumnBottom, false);
            cg.addColour (0.18, rgb (26, 26, 27));
            g.setGradientFill (cg);
            g.fillPath (p);
        };
        column (meterLeftBarX0, meterScaleX0);
        column (meterScaleX1, meterRightBarX1);
    }

    void GainReductionDisplay::paintOverlay (juce::Graphics& g)
    {
        // Centre scale column.
        {
            juce::Path p;
            p.addRoundedRectangle (meterScaleX0, meterColumnTop, meterScaleX1 - meterScaleX0,
                                   meterColumnBottom - meterColumnTop, 9.0f);
            juce::ColourGradient cg (rgb (16, 17, 18, 0.0f), 0.0f, meterColumnTop,
                                     rgb (36, 33, 31, 0.82f), 0.0f, meterColumnBottom, false);
            cg.addColour (0.12, rgb (22, 22, 23, 0.9f));
            g.setGradientFill (cg);
            g.fillPath (p);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawVerticalLine (juce::roundToInt (meterScaleX0), meterColumnTop + 20.0f, meterColumnBottom);
            g.drawVerticalLine (juce::roundToInt (meterScaleX1), meterColumnTop + 20.0f, meterColumnBottom);
        }

        // Ticks at every legend and half-way between legends.
        for (int i = 0; i < 5; ++i)
        {
            const float y = meterDbY[i];
            g.setColour (rgb (92, 94, 97));
            g.fillRect (juce::Rectangle<float> (meterScaleX0 + 4.0f, y - 0.5f, 11.0f, 1.0f));
            g.fillRect (juce::Rectangle<float> (meterScaleX1 - 15.0f, y - 0.5f, 11.0f, 1.0f));
            if (i < 4)
            {
                const float ym = 0.5f * (meterDbY[i] + meterDbY[i + 1]);
                g.setColour (rgb (70, 72, 75));
                g.fillRect (juce::Rectangle<float> (meterScaleX0 + 6.0f, ym - 0.5f, 7.0f, 1.0f));
                g.fillRect (juce::Rectangle<float> (meterScaleX1 - 13.0f, ym - 0.5f, 7.0f, 1.0f));
            }
        }

        // Legends.
        static const char* legends[5] = { "0", "\xe2\x88\x92" "3", "\xe2\x88\x92" "6", "\xe2\x88\x92" "12", "\xe2\x88\x92" "24" };
        static const float inkLeft[5]  { 765.0f, 759.0f, 759.0f, 754.0f, 752.5f };
        static const float inkRight[5] { 775.0f, 780.0f, 780.0f, 782.5f, 784.0f };
        for (int i = 0; i < 5; ++i)
        {
            const float capTop = meterDbY[i] - 6.25f;
            drawTextInInkBox (g, juce::String::fromUTF8 (legends[i]), Weight::medium, colours::meterLegend,
                              inkLeft[i], inkRight[i], capTop, capTop + 12.5f);
        }

        drawTextInInkBox (g, "GAIN REDUCTION", Weight::regular, colours::meterTitle, 692.5f, 846.0f, 195.0f, 206.0f);
    }

    void GainReductionDisplay::paintColumn (juce::Graphics& g, float x0, float x1, float db, bool outerEdgeLeft)
    {
        const float k = heatFor (db);
        if (k <= 0.001f)
            return;

        const float level = yForDb (db);
        const float bottom = meterColumnBottom;
        const float h = std::max (1.0f, bottom - level);
        const float radius = std::min (10.0f, 0.5f * h);

        juce::Path body;
        body.addRoundedRectangle (x0, level, x1 - x0, h, radius, radius, false, false, true, true);

        // Energy ramp fitted to the locked reference: ember at the level,
        // white-hot at the base.
        juce::ColourGradient grad (rgb (54, 34, 32), 0.0f, level, rgb (255, 240, 215), 0.0f, bottom, false);
        grad.addColour (0.088, rgb (71, 41, 33));
        grad.addColour (0.176, rgb (92, 51, 37));
        grad.addColour (0.265, rgb (118, 63, 41));
        grad.addColour (0.353, rgb (143, 75, 44));
        grad.addColour (0.44, rgb (165, 85, 46));
        grad.addColour (0.53, rgb (192, 100, 54));
        grad.addColour (0.62, rgb (212, 114, 61));
        grad.addColour (0.706, rgb (231, 127, 69));
        grad.addColour (0.794, rgb (249, 149, 82));
        grad.addColour (0.88, rgb (253, 177, 113));
        grad.addColour (0.97, rgb (251, 225, 184));
        g.setGradientFill (grad);
        g.setOpacity (k);
        g.fillPath (body);

        // Ember haze just above the level.
        {
            const float hazeTop = std::max (meterColumnTop + 6.0f, level - 36.0f);
            juce::ColourGradient haze (rgb (40, 27, 27, 0.0f), 0.0f, hazeTop, rgb (54, 34, 32, 0.9f * k), 0.0f, level, false);
            g.setGradientFill (haze);
            g.fillRect (juce::Rectangle<float> (x0 + 1.0f, hazeTop, x1 - x0 - 2.0f, level - hazeTop));
        }

        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (body);

            // Neon-tube edges: bright outer wall, softer inner wall.
            auto wall = [&g, level, h, k, bottom] (float x, float width, float strength)
            {
                juce::ColourGradient e (rgb (255, 160, 95, 0.0f), 0.0f, level + 0.15f * h,
                                        rgb (255, 225, 190, strength * k), 0.0f, bottom, false);
                e.addColour (0.5, rgb (250, 140, 80, 0.55f * strength * k));
                g.setGradientFill (e);
                g.fillRect (juce::Rectangle<float> (x, level, width, h));
            };
            wall (outerEdgeLeft ? x0 : x1 - 3.0f, 3.0f, 0.95f);
            wall (outerEdgeLeft ? x1 - 2.0f : x0, 2.0f, 0.45f);

        }

        // White-hot base: the tube's own rounded bottom edge glows.
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (juce::Rectangle<float> (x0 - 2.0f, bottom - 1.6f * radius, x1 - x0 + 4.0f, 2.0f * radius).toNearestInt());
            juce::ColourGradient hot (rgb (255, 236, 210, 0.0f), 0.0f, bottom - 1.6f * radius,
                                      rgb (255, 246, 232, 0.95f * k), 0.0f, bottom, false);
            g.setGradientFill (hot);
            g.strokePath (body, juce::PathStrokeType (2.4f));
        }
        g.setOpacity (1.0f);
    }

    void GainReductionDisplay::paintDynamic (juce::Graphics& g, float leftDb, float rightDb)
    {
        juce::Graphics::ScopedSaveState s (g);
        const auto glass = glassBounds();
        juce::Path glassPath;
        glassPath.addRoundedRectangle (glass, glassRadius());
        g.reduceClipRegion (glassPath);

        const float k = heatFor (std::min (leftDb, rightDb));

        // Faint warmth pooled at the base of the glass.
        if (k > 0.001f)
        {
            const float spillTop = yForDb (std::min (leftDb, rightDb));
            juce::ColourGradient spill (rgb (150, 95, 68, 0.0f), 0.0f, spillTop,
                                        rgb (150, 95, 68, 0.30f * k), 0.0f, glass.getBottom(), false);
            g.setGradientFill (spill);
            g.fillRect (juce::Rectangle<float> (glass.getX(), spillTop, glass.getWidth(), glass.getBottom() - spillTop));

            // Bloom hugging each burning column.
            for (auto [x0, x1] : { std::pair<float, float> { meterLeftBarX0, meterScaleX0 }, { meterScaleX1, meterRightBarX1 } })
            {
                const float top = yForDb (x0 < 700.0f ? leftDb : rightDb) + 30.0f;
                for (int i = 1; i <= 3; ++i)
                {
                    const float e = 3.5f * static_cast<float> (i);
                    juce::ColourGradient bloom (rgb (255, 130, 60, 0.0f), 0.0f, top, rgb (255, 130, 60, 0.07f * k), 0.0f, meterColumnBottom, false);
                    g.setGradientFill (bloom);
                    g.fillRoundedRectangle (juce::Rectangle<float> (x0 - e, top, x1 - x0 + 2 * e, meterColumnBottom - top + e), 9.0f + e);
                }
            }
        }

        paintColumn (g, meterLeftBarX0, meterScaleX0, leftDb, true);
        paintColumn (g, meterScaleX1, meterRightBarX1, rightDb, false);

        // Rim reflection of the glow.
        if (k > 0.001f)
        {
            juce::Path rimLine;
            rimLine.addRoundedRectangle (glass.reduced (0.8f), glassRadius() - 0.8f);
            juce::ColourGradient rg (rgb (255, 160, 90, 0.05f * k), 0.0f, glass.getY(),
                                     rgb (255, 200, 150, 0.85f * k), 0.0f, glass.getBottom(), false);
            rg.addColour (0.45, rgb (255, 150, 80, 0.35f * k));
            g.setGradientFill (rg);
            g.strokePath (rimLine, juce::PathStrokeType (1.6f));
        }
    }

    void GainReductionDisplay::paintNeedle (juce::Graphics& g, float peakDb)
    {
        if (peakDb >= -0.05f)
            return;
        const float y = yForDb (peakDb);
        const float x0 = meterScaleX1 + 1.5f, x1 = meterRightBarX1 - 1.0f;
        g.setColour (rgb (255, 140, 60, 0.18f));
        g.fillRect (juce::Rectangle<float> (x0, y - 5.0f, x1 - x0, 10.0f));
        g.setColour (rgb (255, 150, 70, 0.45f));
        g.fillRect (juce::Rectangle<float> (x0, y - 2.2f, x1 - x0, 4.4f));
        g.setColour (rgb (255, 242, 224));
        g.fillRoundedRectangle (juce::Rectangle<float> (x0, y - 1.1f, x1 - x0, 2.2f), 1.1f);
    }

    void GainReductionDisplay::paint (juce::Graphics& g)
    {
        const float scale = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        if (background.isNull() || std::abs (cachedScale - scale) > 0.01f)
            renderLayers (scale);

        const auto toLocal = juce::AffineTransform::scale (1.0f / cachedScale);
        g.drawImageTransformed (background, toLocal);

        const auto toRef = juce::AffineTransform::translation (-static_cast<float> (getX()), -static_cast<float> (getY()));
        if (! gpuMode)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (toRef);
            paintDynamic (g, shown[0], shown[1]);
        }

        g.drawImageTransformed (overlay, toLocal);

        if (! gpuMode)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (toRef);
            paintNeedle (g, peak);
        }
    }

    std::unique_ptr<juce::AccessibilityHandler> GainReductionDisplay::createAccessibilityHandler()
    {
        class Handler final : public juce::AccessibilityHandler
        {
        public:
            explicit Handler (GainReductionDisplay& d)
                : AccessibilityHandler (d, juce::AccessibilityRole::progressBar, {},
                                        Interfaces { std::make_unique<Value> (d) }) {}

        private:
            class Value final : public juce::AccessibilityTextValueInterface
            {
            public:
                explicit Value (GainReductionDisplay& d) : display (d) {}
                bool isReadOnly() const override { return true; }
                juce::String getCurrentValueAsString() const override
                {
                    return juce::String (std::min (display.getDisplayedDb (0), display.getDisplayedDb (1)), 1) + " dB";
                }
                void setValueAsString (const juce::String&) override {}

            private:
                GainReductionDisplay& display;
            };
        };
        return std::make_unique<Handler> (*this);
    }
}
