#include "UI/GainReductionDisplay.h"
#include "UI/HeatTheme.h"
#include "UI/Layout.h"

namespace heat::ui
{
    using namespace heat::ui::layout;

    namespace
    {
        constexpr float margin = 20.0f; // room for the instrument's shadow on the panel
        constexpr float releaseTau = 0.060f;
        constexpr double peakHoldSeconds = 1.5;
        constexpr float peakFallDbPerSecond = 15.0f;

        // The two glass tubes the embers burn in (x0, x1), and their corner radius.
        constexpr std::pair<float, float> tubes[2] { { meterLeftBarX0, meterScaleX0 }, { meterScaleX1, meterRightBarX1 } };
        constexpr float tubeRadius = 10.0f;
        constexpr float gpuSeam = 1.5f; // inside the bezel's chamfer

        juce::Colour rgb (int r, int g, int b, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::roundToInt (a * 255.0f));
        }

        juce::Colour white (float a) { return juce::Colours::white.withAlpha (a); }

        // Reflections in the cover glass carry the faint cool cast of a coated
        // pane, which sets them apart from the warm light behind it.
        juce::Colour sky (float a) { return juce::Colour (0xffdce8ff).withAlpha (a); }

        juce::Path roundedRect (juce::Rectangle<float> r, float radius)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            return p;
        }

        juce::Rectangle<float> tubeArea (float x0, float x1)
        {
            return { x0, meterColumnTop, x1 - x0, meterColumnBottom - meterColumnTop };
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

    juce::Rectangle<float> GainReductionDisplay::gpuWindowBounds()
    {
        return glassBounds().expanded (gpuSeam);
    }

    float GainReductionDisplay::gpuWindowRadius()
    {
        return glassRadius() + gpuSeam;
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
        // Origin on a multiple of 4 reference units: a whole pixel at 50, 75,
        // 100 and 125 %, so the cached layers are copied 1:1, not resampled.
        const auto area = outer.expanded (margin);
        const int x = 4 * static_cast<int> (std::floor (area.getX() / 4.0f));
        const int y = 4 * static_cast<int> (std::floor (area.getY() / 4.0f));
        setBounds (x, y, static_cast<int> (std::ceil (area.getRight())) - x, static_cast<int> (std::ceil (area.getBottom())) - y);
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
            repaintGlass();
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
                repaintGlass();
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
            paintBackground (g);
        }
        if (gpuMode)
        {
            // Leave the GPU window clear: the OpenGL layer shows through. The
            // window is cut out of the finished layer in one copy; clipping each
            // primitive of the bezel separately would stack their partial
            // coverage on the window's antialiased edge, and the lighter face
            // under the chamfer would bleed into the seam.
            juce::Path hole;
            hole.addRectangle (getBounds().toFloat());
            hole.addRoundedRectangle (gpuWindowBounds(), gpuWindowRadius());
            hole.setUsingNonZeroWinding (false);
            hole.applyTransform (toRef);
            juce::Image cut (juce::Image::ARGB, w, h, true);
            juce::Graphics g (cut);
            g.reduceClipRegion (hole);
            g.drawImageAt (background, 0, 0);
            background = cut;
        }
        overlay = juce::Image (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (overlay);
            g.addTransform (toRef);
            paintOverlay (g);
            paintGlass (g);
        }
    }

    void GainReductionDisplay::paintBackground (juce::Graphics& g)
    {
        const auto outer = meterOuter;
        const auto glass = glassBounds();
        const float glassR = glassRadius();
        const auto housing = roundedRect (outer, meterOuterRadius);

        // Shadow on the panel: a tight contact shadow under a wide ambient
        // one, so the instrument sits on the metal instead of floating.
        juce::DropShadow (juce::Colours::black.withAlpha (0.24f), 16, { 0, 6 }).drawForPath (g, housing);
        juce::DropShadow (juce::Colours::black.withAlpha (0.32f), 3, { 0, 1 }).drawForPath (g, housing);

        // Machined bezel, lit from the top left: polished outer edge, satin
        // face, then a chamfer stepping down to a black seal around the glass.
        {
            juce::ColourGradient face (rgb (238, 239, 241), outer.getX(), outer.getY(),
                                       rgb (180, 176, 174), outer.getRight(), outer.getBottom(), false);
            face.addColour (0.45, rgb (213, 213, 214));
            g.setGradientFill (face);
            g.fillPath (housing);
            g.setColour (rgb (70, 73, 77, 0.9f));
            g.strokePath (housing, juce::PathStrokeType (1.0f));

            juce::ColourGradient edge (white (0.95f), 0.0f, outer.getY(), white (0.30f), 0.0f, outer.getBottom(), false);
            edge.addColour (0.2, white (0.75f));
            g.setGradientFill (edge);
            g.strokePath (roundedRect (outer.reduced (1.2f), meterOuterRadius - 1.2f), juce::PathStrokeType (1.0f));

            // Turned step in the middle of the face.
            juce::ColourGradient step (white (0.55f), 0.0f, outer.getY(), white (0.12f), 0.0f, outer.getBottom(), false);
            g.setGradientFill (step);
            g.strokePath (roundedRect (glass.expanded (4.6f), glassR + 4.6f), juce::PathStrokeType (0.8f));
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.strokePath (roundedRect (glass.expanded (4.0f), glassR + 4.0f), juce::PathStrokeType (0.6f));

            // The chamfer faces down at the top (in shade) and up at the bottom (lit).
            juce::ColourGradient chamfer (rgb (96, 96, 100), 0.0f, glass.getY(), rgb (252, 252, 253), 0.0f, glass.getBottom(), false);
            chamfer.addColour (0.1, rgb (128, 128, 132));
            chamfer.addColour (0.6, rgb (190, 190, 193));
            g.setGradientFill (chamfer);
            g.strokePath (roundedRect (glass.expanded (2.2f), glassR + 2.2f), juce::PathStrokeType (2.6f));

            g.setColour (rgb (6, 6, 7, 0.95f));
            g.strokePath (roundedRect (glass.expanded (0.45f), glassR + 0.45f), juce::PathStrokeType (1.0f));

            // Glints where the light catches the rounded corners.
            juce::Path ring;
            ring.addRoundedRectangle (outer.reduced (0.8f), meterOuterRadius - 0.8f);
            ring.addRoundedRectangle (glass.expanded (3.4f), glassR + 3.4f);
            ring.setUsingNonZeroWinding (false);
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (ring);
            const float d = meterOuterRadius * (1.0f - juce::MathConstants<float>::sqrt2 * 0.5f) + 1.5f;
            auto glint = [&g] (juce::Point<float> c, float radius, float alpha)
            {
                juce::ColourGradient gl (white (alpha), c, white (0.0f), c + juce::Point<float> (radius, 0.0f), true);
                gl.addColour (0.3, white (0.45f * alpha));
                g.setGradientFill (gl);
                g.fillEllipse (juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (c));
            };
            glint (outer.getTopLeft() + juce::Point<float> (d, d), 26.0f, 0.75f);
            glint (outer.getBottomRight() - juce::Point<float> (d, d), 18.0f, 0.3f);
        }

        // Back plate, set deep under the glass.
        const auto glassPath = roundedRect (glass, glassR);
        {
            juce::ColourGradient gg (rgb (8, 8, 9), glass.getX(), glass.getY(),
                                     rgb (24, 20, 18), glass.getX(), glass.getBottom(), false);
            gg.addColour (0.35, rgb (12, 12, 13));
            g.setGradientFill (gg);
            g.fillPath (glassPath);

            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (glassPath);

            // The plate curves away from the middle.
            juce::ColourGradient lift (rgb (255, 236, 222, 0.035f), glass.getCentreX(), glass.getCentreY() + 30.0f,
                                       rgb (255, 236, 222, 0.0f), glass.getCentreX() + 0.7f * glass.getWidth(), glass.getCentreY() + 30.0f, true);
            g.setGradientFill (lift);
            g.fillRect (glass);

            // The bezel walls shade the recess: most at the top, least at the bottom.
            auto shade = [&g, &glass] (juce::Point<float> from, juce::Point<float> to, float alpha)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (alpha), from, juce::Colours::transparentBlack, to, false));
                g.fillRect (glass);
            };
            shade ({ 0.0f, glass.getY() }, { 0.0f, glass.getY() + 30.0f }, 0.75f);
            shade ({ glass.getX(), 0.0f }, { glass.getX() + 14.0f, 0.0f }, 0.45f);
            shade ({ glass.getRight(), 0.0f }, { glass.getRight() - 10.0f, 0.0f }, 0.35f);
            shade ({ 0.0f, glass.getBottom() }, { 0.0f, glass.getBottom() - 6.0f }, 0.25f);
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

        // Empty glass tubes (the lit bodies are drawn on top per frame): round,
        // so their walls fall off into shadow.
        for (auto [x0, x1] : tubes)
        {
            const auto area = tubeArea (x0, x1);
            const auto tube = roundedRect (area, tubeRadius);
            juce::ColourGradient cg (rgb (24, 24, 25, 0.0f), 0.0f, meterColumnTop - 20.0f,
                                     rgb (30, 27, 25), 0.0f, meterColumnBottom, false);
            cg.addColour (0.18, rgb (26, 26, 27));
            g.setGradientFill (cg);
            g.fillPath (tube);

            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (tube);
            juce::ColourGradient round (juce::Colours::black.withAlpha (0.42f), x0, 0.0f, juce::Colours::black.withAlpha (0.5f), x1, 0.0f, false);
            round.addColour (0.3, juce::Colours::transparentBlack);
            round.addColour (0.62, juce::Colours::transparentBlack);
            g.setGradientFill (round);
            g.fillRect (area);
        }
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

    void GainReductionDisplay::paintGlass (juce::Graphics& g)
    {
        const auto glass = glassBounds();
        const float glassR = glassRadius();
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (roundedRect (glass, glassR));

        // Tube glass: a soft sheen and a specular line on the lit side, a
        // faint reflection on the far wall. Visible over embers and empty tube alike.
        for (auto [x0, x1] : tubes)
        {
            const auto area = tubeArea (x0, x1);
            const float w = area.getWidth();
            juce::Graphics::ScopedSaveState ts (g);
            g.reduceClipRegion (roundedRect (area, tubeRadius));

            g.setGradientFill (juce::ColourGradient (white (0.07f), x0 + 0.08f * w, 0.0f, white (0.0f), x0 + 0.55f * w, 0.0f, false));
            g.fillRect (area);

            juce::ColourGradient spec (white (0.0f), 0.0f, area.getY() + 2.0f, white (0.0f), 0.0f, area.getBottom() - 2.0f, false);
            spec.addColour (0.05, white (0.30f));
            spec.addColour (0.45, white (0.13f));
            spec.addColour (0.92, white (0.18f));
            g.setGradientFill (spec);
            g.fillRoundedRectangle ({ x0 + 0.22f * w, area.getY() + 3.0f, 2.2f, area.getHeight() - 6.0f }, 1.1f);

            juce::ColourGradient far (white (0.0f), 0.0f, area.getY() + 6.0f, white (0.0f), 0.0f, area.getBottom() - 6.0f, false);
            far.addColour (0.2, white (0.08f));
            far.addColour (0.8, white (0.06f));
            g.setGradientFill (far);
            g.fillRect (juce::Rectangle<float> (x1 - 3.2f, area.getY(), 1.0f, area.getHeight()));
        }

        const float w = glass.getWidth(), h = glass.getHeight();

        // Cover glass. A softbox reflected in the top-left corner ...
        {
            const auto hot = glass.getTopLeft() + juce::Point<float> (0.2f * w, 0.05f * h);
            juce::ColourGradient box (sky (0.11f), hot, sky (0.0f), hot + juce::Point<float> (0.62f * w, 0.0f), true);
            box.addColour (0.45, sky (0.035f));
            g.setGradientFill (box);
            g.fillRect (glass);
        }

        // ... the crystal's curved reflection sweeping over the top of the pane ...
        {
            const float x0 = glass.getX() - 2.0f, x1 = glass.getRight() + 2.0f, top = glass.getY() - 2.0f;
            juce::Path sweep;
            sweep.startNewSubPath (x0, top);
            sweep.lineTo (x1, top);
            sweep.lineTo (x1, glass.getY() + 0.2f * h);
            sweep.cubicTo (x1 - 0.35f * w, glass.getY() + 0.29f * h, x0 + 0.35f * w, glass.getY() + 0.36f * h, x0, glass.getY() + 0.46f * h);
            sweep.closeSubPath();
            g.setGradientFill (juce::ColourGradient (sky (0.07f), glass.getX(), glass.getY(),
                                                     sky (0.014f), glass.getX() + 0.12f * w, glass.getY() + 0.45f * h, false));
            g.fillPath (sweep);
        }

        // ... more reflection at grazing angles under the bezel ...
        g.setGradientFill (juce::ColourGradient (sky (0.08f), 0.0f, glass.getY(), sky (0.0f), 0.0f, glass.getY() + 16.0f, false));
        g.fillRect (glass);

        // ... and the polished edge of the pane catching the light.
        juce::ColourGradient edge (white (0.45f), glass.getX(), glass.getY(), white (0.05f), glass.getRight(), glass.getBottom(), false);
        edge.addColour (0.5, white (0.12f));
        g.setGradientFill (edge);
        g.strokePath (roundedRect (glass.reduced (1.9f), glassR - 1.9f), juce::PathStrokeType (1.0f));
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

        // Everything that moves sits under the scale and the cover glass.
        if (! gpuMode)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (juce::AffineTransform::translation (-static_cast<float> (getX()), -static_cast<float> (getY())));
            paintDynamic (g, shown[0], shown[1]);
            paintNeedle (g, peak);
        }

        g.drawImageTransformed (overlay, toLocal);
    }

    void GainReductionDisplay::repaintGlass()
    {
        // Everything that animates is clipped to the glass.
        repaint (glassBounds().translated (-static_cast<float> (getX()), -static_cast<float> (getY())).getSmallestIntegerContainer().expanded (1));
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
