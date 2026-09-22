#include "Graphics/SilverSurface.h"
#include "UI/HeatTheme.h"
#include "UI/Icons.h"
#include "UI/Layout.h"

#include <random>

namespace heat::ui::surface
{
    using namespace heat::ui::layout;
    namespace col = heat::ui::colours;

    namespace
    {
        juce::Colour grey (int v, float a = 1.0f)
        {
            return juce::Colour::fromRGBA ((juce::uint8) v, (juce::uint8) std::min (255, v + 2),
                                           (juce::uint8) std::min (255, v + 4), (juce::uint8) juce::roundToInt (a * 255.0f));
        }

        void fillVerticalStops (juce::Graphics& g, const juce::Path& area, float y0, float y1,
                                std::initializer_list<std::pair<float, int>> stops)
        {
            juce::ColourGradient grad (grey (stops.begin()->second), 0.0f, y0, grey ((stops.end() - 1)->second), 0.0f, y1, false);
            for (const auto& s : stops)
                grad.addColour ((s.first - y0) / (y1 - y0), grey (s.second));
            g.setGradientFill (grad);
            g.fillPath (area);
        }

        void fadingLine (juce::Graphics& g, float x0, float x1, float y, float thickness, juce::Colour c,
                         float fadeLeft, float fadeRight)
        {
            juce::ColourGradient grad (c.withAlpha (0.0f), x0, y, c.withAlpha (0.0f), x1, y, false);
            const float w = x1 - x0;
            grad.addColour (juce::jlimit (0.0, 1.0, (double) (fadeLeft / w)), c);
            grad.addColour (juce::jlimit (0.0, 1.0, (double) (1.0f - fadeRight / w)), c);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x0, y - 0.5f * thickness, w, thickness));
        }

        void verticalDivider (juce::Graphics& g, float x, float y0, float y1)
        {
            juce::ColourGradient grad (juce::Colours::black.withAlpha (0.0f), x, y0,
                                       juce::Colours::black.withAlpha (0.0f), x, y1, false);
            grad.addColour (0.12, juce::Colours::black.withAlpha (0.30f));
            grad.addColour (0.88, juce::Colours::black.withAlpha (0.30f));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x - 0.6f, y0, 1.2f, y1 - y0));
            juce::ColourGradient hi (juce::Colours::white.withAlpha (0.0f), x, y0,
                                     juce::Colours::white.withAlpha (0.0f), x, y1, false);
            hi.addColour (0.12, juce::Colours::white.withAlpha (0.35f));
            hi.addColour (0.88, juce::Colours::white.withAlpha (0.35f));
            g.setGradientFill (hi);
            g.fillRect (juce::Rectangle<float> (x + 0.6f, y0, 0.9f, y1 - y0));
        }

        void text (juce::Graphics& g, const juce::String& s, Weight w, juce::Colour c,
                   float x0, float x1, float capTop, float capBottom)
        {
            drawTextInInkBox (g, s, w, c, x0, x1, capTop, capBottom);
        }

        void strokeIcon (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::translation (0.0f, 0.8f));
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    const juce::Image& satinNoiseTile()
    {
        static const juce::Image tile = []
        {
            const int w = 512, h = 256;
            std::mt19937 rng (0x5eed);
            std::normal_distribution<float> nd (0.0f, 1.0f);
            std::vector<float> raw (static_cast<size_t> (w * h));
            // Per-row streak offset + per-pixel grain, then a horizontal blur.
            for (int y = 0; y < h; ++y)
            {
                const float row = 0.6f * nd (rng);
                for (int x = 0; x < w; ++x)
                    raw[static_cast<size_t> (y * w + x)] = row + nd (rng);
            }
            std::vector<float> blurred (raw.size());
            const int radius = 6;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    float s = 0.0f;
                    for (int k = -radius; k <= radius; ++k)
                        s += raw[static_cast<size_t> (y * w + (x + k + w) % w)];
                    blurred[static_cast<size_t> (y * w + x)] = s / (2 * radius + 1);
                }

            juce::Image img (juce::Image::ARGB, w, h, true);
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float v = blurred[static_cast<size_t> (y * w + x)] * 2.2f;
                    // White over ~200 grey lifts by a*55, black lowers by a*200: scale the
                    // dark half so the surface's mean brightness is preserved.
                    const float strength = v > 0 ? 40.0f : 40.0f * 0.28f;
                    const auto a = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (std::abs (v) * strength));
                    bd.setPixelColour (x, y, v > 0 ? juce::Colour::fromRGBA (255, 255, 255, a)
                                                   : juce::Colour::fromRGBA (0, 0, 0, a));
                }
            return img;
        }();
        return tile;
    }

    juce::Image makeSpunFace (int diameterPx, const SpunFaceStyle& style)
    {
        const int d = std::max (4, diameterPx);
        juce::Image img (juce::Image::ARGB, d, d, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);

        std::mt19937 rng (style.seed);
        std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
        const int rings = d; // one grain value per half-pixel of radius
        std::vector<float> grain (static_cast<size_t> (rings + 2));
        float smooth = 0.0f;
        for (auto& gv : grain)
        {
            smooth = 0.55f * smooth + 0.45f * uni (rng);
            gv = smooth;
        }

        const float r = 0.5f * static_cast<float> (d);
        for (int y = 0; y < d; ++y)
            for (int x = 0; x < d; ++x)
            {
                const float dx = static_cast<float> (x) + 0.5f - r;
                const float dy = static_cast<float> (y) + 0.5f - r;
                const float dist = std::sqrt (dx * dx + dy * dy);
                if (dist > r + 1.0f)
                    continue;

                const float theta = std::atan2 (dx, -dy); // 0 at 12 o'clock, clockwise
                float v = style.base + style.conic * std::cos (2.0f * theta) + style.tilt * std::cos (theta);
                v += style.grain * grain[static_cast<size_t> (std::min (rings, (int) (dist * 2.0f)))];

                const float coverage = juce::jlimit (0.0f, 1.0f, r - dist + 0.5f);
                const int iv = juce::jlimit (0, 255, juce::roundToInt (v));
                bd.setPixelColour (x, y, juce::Colour::fromRGBA ((juce::uint8) iv,
                                                                  (juce::uint8) std::min (255, iv + 1),
                                                                  (juce::uint8) std::min (255, iv + 3),
                                                                  (juce::uint8) juce::roundToInt (coverage * 255.0f)));
            }
        return img;
    }

    void drawGroove (juce::Graphics& g, float x0, float x1, float y, float fadeLeft, float fadeRight,
                     float darkAlpha, float lightAlpha)
    {
        fadingLine (g, x0, x1, y + 0.5f, 1.2f, juce::Colours::black.withAlpha (darkAlpha), fadeLeft, fadeRight);
        fadingLine (g, x0, x1, y + 1.7f, 1.1f, juce::Colours::white.withAlpha (lightAlpha), fadeLeft, fadeRight);
    }

    void paintChassis (juce::Graphics& g)
    {
        // ---- Backdrop -------------------------------------------------------
        g.fillAll (juce::Colour (0xfff2f4f4));
        {
            // Floor shadow under the chassis.
            juce::ColourGradient shadow (juce::Colour (0xffd8dadb), 0.0f, 0.0f, juce::Colour (0xffd8dadb), canvasWidth, 0.0f, false);
            shadow.addColour (0.03, juce::Colour (0xff5d5f61));
            shadow.addColour (0.5, juce::Colour (0xff57585a));
            shadow.addColour (0.97, juce::Colour (0xff545557));
            g.setGradientFill (shadow);
            g.fillRect (juce::Rectangle<float> (0.0f, chassisBottom - 14.0f, canvasWidth, canvasHeight - chassisBottom + 14.0f));
        }

        // ---- Chassis body ------------------------------------------------------
        juce::Path chassis;
        chassis.addRoundedRectangle (0.0f, 0.0f, canvasWidth, chassisBottom, chassisRadius);
        g.setColour (grey (150));
        g.fillPath (chassis);

        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (chassis);

            // Top rim.
            juce::Path rim;
            rim.addRectangle (0.0f, 0.0f, canvasWidth, faceTop + 2.0f);
            fillVerticalStops (g, rim, 0.0f, faceTop, { { 0.0f, 214 }, { 2.0f, 208 }, { 10.0f, 203 }, { 17.0f, 198 } });
            g.setTiledImageFill (satinNoiseTile(), 0, 0, 0.2f);
            g.fillPath (rim);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> (0.0f, 1.0f, canvasWidth, 1.0f));

            // Lip (front edge below the face).
            juce::Path lip;
            lip.addRectangle (0.0f, faceBottom, canvasWidth, chassisBottom - faceBottom);
            juce::ColourGradient lipGrad (grey (127), 0.0f, 0.0f, grey (128), canvasWidth, 0.0f, false);
            lipGrad.addColour (0.08, grey (140));
            lipGrad.addColour (0.22, grey (160));
            lipGrad.addColour (0.36, grey (186));
            lipGrad.addColour (0.47, grey (203));
            lipGrad.addColour (0.55, grey (200));
            lipGrad.addColour (0.66, grey (184));
            lipGrad.addColour (0.80, grey (162));
            lipGrad.addColour (0.92, grey (141));
            g.setGradientFill (lipGrad);
            g.fillPath (lip);
            juce::ColourGradient lipShade (juce::Colours::white.withAlpha (0.10f), 0.0f, faceBottom,
                                           juce::Colours::black.withAlpha (0.10f), 0.0f, chassisBottom, false);
            g.setGradientFill (lipShade);
            g.fillPath (lip);
            g.setTiledImageFill (satinNoiseTile(), 0, 3, 0.45f);
            g.fillPath (lip);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (0.0f, chassisBottom - 2.5f, canvasWidth, 1.0f));
        }
        g.setColour (juce::Colour (0xff3b3e42));
        g.strokePath (chassis, juce::PathStrokeType (1.2f));

        // ---- Face panel -----------------------------------------------------------
        juce::Path face;
        face.addRoundedRectangle (faceInsetX, faceTop, canvasWidth - 2.0f * faceInsetX, faceBottom - faceTop, faceRadius);
        fillVerticalStops (g, face, faceTop, faceBottom,
                           { { faceTop, 212 }, { 150.0f, 206 }, { 300.0f, 202 }, { 550.0f, 195 }, { 750.0f, 189 }, { faceBottom, 181 } });
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (face);

            juce::ColourGradient glowTop (juce::Colours::white.withAlpha (0.13f), 768.0f, 190.0f,
                                          juce::Colours::white.withAlpha (0.0f), 768.0f + 700.0f, 190.0f, true);
            g.setGradientFill (glowTop);
            g.fillPath (face);
            juce::ColourGradient glowLow (juce::Colours::white.withAlpha (0.14f), 768.0f, 900.0f,
                                          juce::Colours::white.withAlpha (0.0f), 768.0f + 330.0f, 900.0f, true);
            g.setGradientFill (glowLow);
            g.fillPath (face);
            // Slightly darker outer thirds (the reference falls off towards the sides).
            juce::ColourGradient sides (juce::Colours::black.withAlpha (0.05f), 0.0f, 0.0f,
                                        juce::Colours::black.withAlpha (0.06f), canvasWidth, 0.0f, false);
            sides.addColour (0.3, juce::Colours::black.withAlpha (0.0f));
            sides.addColour (0.7, juce::Colours::black.withAlpha (0.0f));
            g.setGradientFill (sides);
            g.fillPath (face);

            g.setTiledImageFill (satinNoiseTile(), 0, 0, 0.16f);
            g.fillPath (face);

            // Inner top highlight and bottom bevel.
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (0.0f, faceTop + 2.0f, canvasWidth, 1.3f));
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (0.0f, faceTop + 3.3f, canvasWidth, 1.0f));
        }
        g.setColour (juce::Colour (0xff2c3034));
        g.strokePath (face, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (faceInsetX + 8.0f, faceBottom + 1.0f, canvasWidth - 2 * (faceInsetX + 8.0f), 1.2f));

        // ---- Separators ---------------------------------------------------------------
        fadingLine (g, 24.0f, 633.0f, headerLineY + 0.5f, 1.2f, juce::Colours::black.withAlpha (0.17f), 2.0f, 60.0f);
        fadingLine (g, 899.0f, 1510.0f, headerLineY + 0.5f, 1.2f, juce::Colours::black.withAlpha (0.17f), 60.0f, 2.0f);
        fadingLine (g, 24.0f, 633.0f, headerLineY + 1.6f, 1.0f, juce::Colours::black.withAlpha (0.06f), 2.0f, 60.0f);
        fadingLine (g, 899.0f, 1510.0f, headerLineY + 1.6f, 1.0f, juce::Colours::black.withAlpha (0.06f), 60.0f, 2.0f);

        drawGroove (g, 23.0f, 629.0f, midGrooveY, 2.0f, 70.0f, 0.62f, 0.75f);
        drawGroove (g, 904.0f, 1511.0f, midGrooveY, 70.0f, 2.0f, 0.62f, 0.75f);
        drawGroove (g, 24.0f, 598.0f, lowGrooveY, 2.0f, 60.0f, 0.60f, 0.80f);
        drawGroove (g, 935.0f, 1511.0f, lowGrooveY, 60.0f, 2.0f, 0.60f, 0.80f);

        // Section underlines (MODE / DETECTOR).
        fadingLine (g, 75.0f, 523.0f, 630.3f, 1.1f, juce::Colours::black.withAlpha (0.22f), 4.0f, 4.0f);
        fadingLine (g, 75.0f, 523.0f, 631.4f, 1.0f, juce::Colours::white.withAlpha (0.55f), 4.0f, 4.0f);
        fadingLine (g, 1046.7f, 1460.0f, 630.3f, 1.1f, juce::Colours::black.withAlpha (0.22f), 4.0f, 4.0f);
        fadingLine (g, 1046.7f, 1460.0f, 631.4f, 1.0f, juce::Colours::white.withAlpha (0.55f), 4.0f, 4.0f);

        for (float x : bottomDividerX)
            verticalDivider (g, x, bottomDividerTop, bottomDividerBottom);

        // ---- Header ---------------------------------------------------------------------
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillPath (icons::logo(), juce::AffineTransform::translation (0.0f, 1.0f));
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (icons::logoLambdaClip().translated (0.0f, 1.0f).toNearestInt());
                g.fillPath (icons::logoLambda(), juce::AffineTransform::translation (0.0f, 1.0f));
            }
            g.setColour (col::ink);
            g.fillPath (icons::logo());
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (icons::logoLambdaClip().toNearestInt());
                g.fillPath (icons::logoLambda());
            }
        }
        g.setColour (juce::Colour (0xffa9adb1));
        g.fillRect (juce::Rectangle<float> (logoDividerX - 0.5f, 63.0f, 1.0f, 40.0f));
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (logoDividerX + 0.5f, 63.0f, 0.8f, 40.0f));

        text (g, "ANALOG DYNAMICS", Weight::regular, juce::Colour (0xff484c51), 313.3f, 471.7f, 69.3f, 78.3f);
        text (g, "PROCESSOR", Weight::regular, juce::Colour (0xff484c51), 313.3f, 411.0f, 91.0f, 100.3f);

        g.setColour (juce::Colour (0xffa9adb1));
        g.fillRect (juce::Rectangle<float> (headerDividerX - 0.5f, 65.0f, 1.0f, 38.0f));
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (headerDividerX + 0.5f, 65.0f, 0.8f, 38.0f));

        text (g, "BY NOVA AUDIO", Weight::medium, juce::Colour (0xff34383d), 1346.0f, 1479.0f, 71.7f, 81.7f);
        fadingLine (g, 1403.3f, 1478.3f, 102.3f, 1.8f, juce::Colour (0xff6c7075), 6.0f, 6.0f);

        // ---- Knob legends --------------------------------------------------------------
        const auto labelColour = juce::Colour (0xff22262a);
        const auto legendColour = juce::Colour (0xff33373b);
        text (g, "INPUT", Weight::medium, labelColour, 162.3f, 225.7f, 205.0f, 216.7f);
        text (g, "OUTPUT", Weight::medium, labelColour, 1300.0f, 1382.5f, 205.0f, 217.2f);
        text (g, "ATTACK", Weight::medium, labelColour, 440.0f, 511.3f, 281.3f, 292.5f);
        text (g, "RELEASE", Weight::medium, labelColour, 1022.5f, 1105.0f, 281.5f, 292.5f);

        text (g, "FAST", Weight::medium, legendColour, 392.5f, 427.0f, 456.0f, 466.0f);
        text (g, "SLOW", Weight::medium, legendColour, 520.5f, 560.0f, 456.0f, 466.0f);
        text (g, "FAST", Weight::medium, legendColour, 980.0f, 1015.0f, 456.5f, 466.5f);
        text (g, "SLOW", Weight::medium, legendColour, 1107.5f, 1147.5f, 456.5f, 466.5f);

        text (g, "-24", Weight::medium, legendColour, 80.0f, 108.0f, 496.7f, 506.7f);
        text (g, "0", Weight::medium, legendColour, 183.3f, 190.3f, 503.3f, 513.5f);
        text (g, "+24", Weight::medium, legendColour, 260.7f, 289.3f, 497.0f, 507.0f);
        text (g, "-24", Weight::medium, legendColour, 1250.0f, 1277.5f, 496.5f, 506.8f);
        text (g, "0", Weight::medium, legendColour, 1346.5f, 1353.8f, 503.3f, 513.5f);
        text (g, "+24", Weight::medium, legendColour, 1424.0f, 1452.5f, 497.0f, 507.2f);

        // End-of-range ticks beside the INPUT / OUTPUT legends.
        auto tick = [&g] (juce::Point<float> c, float angleDeg, float r0, float r1)
        {
            const float a = juce::degreesToRadians (angleDeg);
            juce::Path p;
            p.startNewSubPath (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a));
            p.lineTo (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a));
            g.setColour (juce::Colour (0xff4a4e53));
            g.strokePath (p, juce::PathStrokeType (1.2f));
        };
        tick (inputCentre, -142.0f, 127.0f, 134.0f);
        tick (inputCentre, 142.0f, 127.0f, 134.0f);
        tick (outputCentre, -142.0f, 127.0f, 134.0f);
        tick (outputCentre, 142.0f, 127.0f, 134.0f);

        // ---- Selector captions ------------------------------------------------------------
        text (g, "MODE", Weight::regular, juce::Colour (0xff2a2e32), 81.7f, 136.0f, 608.3f, 619.3f);
        text (g, "DETECTOR", Weight::regular, juce::Colour (0xff2a2e32), 1056.7f, 1157.7f, 607.3f, 619.0f);

        // ---- COMPRESS legends -------------------------------------------------------------
        text (g, "LESS", Weight::medium, legendColour, 597.3f, 633.3f, 788.0f, 798.0f);
        text (g, "MORE", Weight::medium, legendColour, 901.0f, 940.0f, 788.0f, 798.0f);
        text (g, "COMPRESS", Weight::medium, juce::Colour (0xff1e2226), 695.0f, 842.7f, 850.7f, 866.5f);

        // ---- Bottom row ----------------------------------------------------------------------
        const auto bottomLabel = juce::Colour (0xff1f2327);
        text (g, "TUBE", Weight::semibold, bottomLabel, 69.5f, 115.5f, 882.0f, 893.7f);
        text (g, "IRON", Weight::semibold, bottomLabel, 333.5f, 378.5f, 882.3f, 893.7f);
        text (g, "MIX", Weight::semibold, bottomLabel, 1031.5f, 1063.5f, 882.3f, 893.7f);
        text (g, "HPF", Weight::semibold, bottomLabel, 1298.5f, 1332.5f, 882.3f, 893.7f);

        text (g, "0", Weight::medium, legendColour, 144.5f, 151.3f, 885.0f, 894.0f);
        text (g, "100", Weight::medium, legendColour, 222.0f, 245.0f, 885.0f, 894.0f);
        text (g, "0", Weight::medium, legendColour, 404.0f, 412.5f, 885.0f, 894.0f);
        text (g, "100", Weight::medium, legendColour, 481.0f, 506.0f, 885.0f, 894.0f);
        text (g, "DRY", Weight::medium, legendColour, 1083.5f, 1111.0f, 884.5f, 893.5f);
        text (g, "WET", Weight::medium, legendColour, 1177.5f, 1207.5f, 884.5f, 893.5f);
        text (g, "20", Weight::medium, legendColour, 1364.0f, 1380.0f, 885.0f, 894.0f);
        text (g, "400", Weight::medium, legendColour, 1447.5f, 1474.0f, 885.0f, 894.0f);

        const auto iconColour = juce::Colour (0xff4d5257);
        strokeIcon (g, icons::tubeWaves ({ 75.0f, 822.5f, 34.0f, 26.5f }), 1.5f, iconColour);
        strokeIcon (g, icons::ironCoils ({ 340.0f, 823.5f, 30.0f, 26.0f }), 1.5f, iconColour);
        strokeIcon (g, icons::crosshair ({ 1048.0f, 835.0f }, 10.5f), 1.4f, iconColour);
        strokeIcon (g, icons::highPassCurve ({ 1299.0f, 822.5f, 32.0f, 27.5f }), 1.6f, iconColour);

        // ---- Footer ------------------------------------------------------------------------------
        const auto footer = juce::Colour (0xff4c5054);
        text (g, "COMPRESSION CREATES THE MOTION", Weight::regular, footer, 385.0f, 703.0f, 965.0f, 975.0f);
        text (g, "/", Weight::regular, footer, 736.0f, 741.0f, 964.0f, 976.0f);
        text (g, "ANALOG COLOR CREATES THE EMOTION", Weight::regular, footer, 778.0f, 1136.5f, 965.0f, 975.0f);
    }
}
