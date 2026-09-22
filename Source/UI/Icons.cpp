#include "UI/Icons.h"
#include "UI/Layout.h"

namespace heat::ui::icons
{
    using namespace heat::ui::layout;

    juce::Path logo()
    {
        const float t = logoTop, b = logoBottom, s = logoStroke;
        const float mid = 0.5f * (t + b) - 0.5f * s + 0.35f;
        juce::Path p;

        // H
        p.addRectangle (logoH0, t, s, b - t);
        p.addRectangle (logoH1 - s, t, s, b - t);
        p.addRectangle (logoH0, mid, logoH1 - logoH0, s);

        // E — detached top bar, middle bar, stem only in the lower half, bottom bar.
        p.addRectangle (logoE0, t, logoE1 - logoE0, s);
        p.addRectangle (logoE0, mid, logoE1 - logoE0 - 4.0f, s);
        p.addRectangle (logoE0, mid, s, b - mid);
        p.addRectangle (logoE0, b - s, logoE1 - logoE0, s);

        // T
        p.addRectangle (logoT0, t, logoT1 - logoT0, s);
        p.addRectangle (0.5f * (logoT0 + logoT1) - 0.5f * s + 0.3f, t, s, b - t);
        return p;
    }

    juce::Path logoLambda()
    {
        // Two legs meeting in a sharp apex. The feet extend below the letter
        // box; painting through logoLambdaClip() gives them flat bottoms.
        juce::Path legs;
        const float apexX = 0.5f * (logoA0 + logoA1);
        legs.startNewSubPath (logoA0 + 1.2f, logoBottom + 6.0f);
        legs.lineTo (apexX, logoTop + 2.4f);
        legs.lineTo (logoA1 - 1.2f, logoBottom + 6.0f);
        juce::Path stroked;
        juce::PathStrokeType (logoStroke, juce::PathStrokeType::mitered, juce::PathStrokeType::butt)
            .createStrokedPath (stroked, legs);
        return stroked;
    }

    juce::Rectangle<float> logoLambdaClip()
    {
        return { logoA0 - 6.0f, logoTop - 2.0f, logoA1 - logoA0 + 12.0f, logoBottom - logoTop + 2.0f };
    }

    juce::Path gear (juce::Point<float> c, float size)
    {
        // Eight softly rounded points around a ring (reads as a sun / gear).
        juce::Path star;
        const int points = 8;
        const float outer = 0.5f * size, inner = outer * 0.80f;
        for (int i = 0; i < points * 2; ++i)
        {
            const float r = (i % 2 == 0) ? outer : inner;
            const float a = juce::MathConstants<float>::pi * static_cast<float> (i) / points;
            const juce::Point<float> pt (c.x + r * std::sin (a), c.y - r * std::cos (a));
            if (i == 0)
                star.startNewSubPath (pt);
            else
                star.lineTo (pt);
        }
        star.closeSubPath();
        return star.createPathWithRoundedCorners (size * 0.09f);
    }

    juce::Path heart (juce::Point<float> c, float w, float h)
    {
        juce::Path p;
        const float x = c.x - 0.5f * w, y = c.y - 0.5f * h;
        p.startNewSubPath (c.x, y + h);
        p.cubicTo (x + w * 0.10f, y + h * 0.62f, x - w * 0.02f, y + h * 0.30f, x + w * 0.10f, y + h * 0.13f);
        p.cubicTo (x + w * 0.22f, y - h * 0.03f, x + w * 0.44f, y + h * 0.00f, c.x, y + h * 0.22f);
        p.cubicTo (x + w * 0.56f, y + h * 0.00f, x + w * 0.78f, y - h * 0.03f, x + w * 0.90f, y + h * 0.13f);
        p.cubicTo (x + w * 1.02f, y + h * 0.30f, x + w * 0.90f, y + h * 0.62f, c.x, y + h);
        p.closeSubPath();
        return p;
    }

    juce::Path chevron (juce::Point<float> c, float w, float h, bool pointsLeft)
    {
        juce::Path p;
        const float dx = pointsLeft ? 0.5f * w : -0.5f * w;
        p.startNewSubPath (c.x + dx, c.y - 0.5f * h);
        p.lineTo (c.x - dx, c.y);
        p.lineTo (c.x + dx, c.y + 0.5f * h);
        return p;
    }

    juce::Path tubeWaves (juce::Rectangle<float> area)
    {
        juce::Path p;
        const int lines = 4;
        const float amplitude = 2.1f;
        const float periods = 2.6f;
        for (int l = 0; l < lines; ++l)
        {
            const float y = area.getY() + 2.0f + (area.getHeight() - 4.0f) * static_cast<float> (l) / (lines - 1);
            for (int i = 0; i <= 48; ++i)
            {
                const float u = static_cast<float> (i) / 48.0f;
                const float x = area.getX() + u * area.getWidth();
                const float yy = y + amplitude * std::sin (u * periods * juce::MathConstants<float>::twoPi + 0.4f);
                if (i == 0)
                    p.startNewSubPath (x, yy);
                else
                    p.lineTo (x, yy);
            }
        }
        return p;
    }

    juce::Path ironCoils (juce::Rectangle<float> a)
    {
        // Transformer core drawn as two back-to-back brackets "] [", each
        // holding a small winding.
        juce::Path p;
        const float top = a.getY(), bottom = a.getBottom(), cx = a.getCentreX();
        const float spine = 3.0f;
        p.startNewSubPath (a.getX(), top);
        p.lineTo (cx - spine, top);
        p.lineTo (cx - spine, bottom);
        p.lineTo (a.getX(), bottom);
        p.startNewSubPath (a.getRight(), top);
        p.lineTo (cx + spine, top);
        p.lineTo (cx + spine, bottom);
        p.lineTo (a.getRight(), bottom);

        auto winding = [&p, top, bottom] (float x)
        {
            const float y0 = top + 0.25f * (bottom - top), y1 = bottom - 0.24f * (bottom - top);
            const int bumps = 3;
            const float step = (y1 - y0) / bumps;
            p.startNewSubPath (x, y0);
            for (int i = 0; i < bumps; ++i)
                p.quadraticTo (x - 2.6f, y0 + step * (static_cast<float> (i) + 0.5f), x, y0 + step * static_cast<float> (i + 1));
        };
        winding (a.getX() + 0.45f * (cx - spine - a.getX()) + 1.5f);
        winding (cx + spine + 0.55f * (a.getRight() - cx - spine) + 1.5f);
        return p;
    }

    juce::Path crosshair (juce::Point<float> c, float r)
    {
        juce::Path p;
        p.addEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        const float ri = r * 0.40f;
        p.addEllipse (c.x - ri, c.y - ri, 2 * ri, 2 * ri);
        const float a = r * 0.55f, b = r * 1.38f;
        p.startNewSubPath (c.x, c.y - b); p.lineTo (c.x, c.y - a);
        p.startNewSubPath (c.x, c.y + a); p.lineTo (c.x, c.y + b);
        p.startNewSubPath (c.x - b, c.y); p.lineTo (c.x - a, c.y);
        p.startNewSubPath (c.x + a, c.y); p.lineTo (c.x + b, c.y);
        return p;
    }

    juce::Path highPassCurve (juce::Rectangle<float> a)
    {
        juce::Path p;
        p.startNewSubPath (a.getX(), a.getBottom());
        p.cubicTo (a.getX() + a.getWidth() * 0.06f, a.getY() + a.getHeight() * 0.35f,
                   a.getX() + a.getWidth() * 0.30f, a.getY() + a.getHeight() * 0.02f,
                   a.getX() + a.getWidth() * 0.62f, a.getY() + a.getHeight() * 0.01f);
        p.lineTo (a.getRight(), a.getY());
        return p;
    }
}
