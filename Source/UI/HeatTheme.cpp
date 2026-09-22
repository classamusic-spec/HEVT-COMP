#include "UI/HeatTheme.h"
#include "BinaryData.h"

namespace heat::ui
{
    namespace
    {
        struct Typefaces
        {
            Typefaces()
            {
                light    = juce::Typeface::createSystemTypefaceFor (HeatBinary::InterLight_ttf,    HeatBinary::InterLight_ttfSize);
                regular  = juce::Typeface::createSystemTypefaceFor (HeatBinary::InterRegular_ttf,  HeatBinary::InterRegular_ttfSize);
                medium   = juce::Typeface::createSystemTypefaceFor (HeatBinary::InterMedium_ttf,   HeatBinary::InterMedium_ttfSize);
                semibold = juce::Typeface::createSystemTypefaceFor (HeatBinary::InterSemiBold_ttf, HeatBinary::InterSemiBold_ttfSize);

                for (auto* t : { &light, &regular, &medium, &semibold })
                {
                    // Cap height per unit font height, measured from the outline of "H".
                    juce::Font f (juce::FontOptions (*t).withHeight (100.0f));
                    juce::GlyphArrangement ga;
                    ga.addLineOfText (f, "H", 0.0f, 0.0f);
                    juce::Path p;
                    ga.createPath (p);
                    capRatio[t - &light] = p.getBounds().getHeight() / 100.0f;
                }
            }

            juce::Typeface::Ptr light, regular, medium, semibold;
            float capRatio[4] { 0.6f, 0.6f, 0.6f, 0.6f };
        };

        Typefaces& faces()
        {
            static Typefaces t;
            return t;
        }

        int index (Weight w) { return static_cast<int> (w); }
    }

    juce::Typeface::Ptr typeface (Weight w)
    {
        auto& f = faces();
        switch (w)
        {
            case Weight::light:    return f.light;
            case Weight::regular:  return f.regular;
            case Weight::semibold: return f.semibold;
            case Weight::medium:
            default:               return f.medium;
        }
    }

    juce::Font fontForCapHeight (Weight w, float capHeight)
    {
        const float height = capHeight / faces().capRatio[index (w)];
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (height));
    }

    namespace
    {
        juce::GlyphArrangement arrange (const juce::String& text, const TextSpec& spec)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (fontForCapHeight (spec.weight, spec.capHeight), text, 0.0f, 0.0f);
            if (std::abs (spec.tracking) > 1.0e-6f)
                for (int i = 1; i < ga.getNumGlyphs(); ++i)
                    ga.moveRangeOfGlyphs (i, 1, spec.tracking * static_cast<float> (i), 0.0f);
            return ga;
        }
    }

    juce::Path textPath (const juce::String& text, const TextSpec& spec)
    {
        juce::Path p;
        arrange (text, spec).createPath (p);
        return p;
    }

    void drawText (juce::Graphics& g, const juce::String& text, const TextSpec& spec,
                   float x, float capTop, juce::Justification justification)
    {
        auto p = textPath (text, spec);
        const auto ink = p.getBounds();
        float dx = x - ink.getX();
        if (justification.testFlags (juce::Justification::horizontallyCentred))
            dx = x - ink.getCentreX();
        else if (justification.testFlags (juce::Justification::right))
            dx = x - ink.getRight();

        p.applyTransform (juce::AffineTransform::translation (dx, capTop + spec.capHeight));
        g.setColour (spec.colour);
        g.fillPath (p);
    }

    float trackingForInkWidth (const juce::String& text, Weight weight, float capHeight, float inkWidth)
    {
        TextSpec spec { weight, capHeight, 0.0f, {} };
        const auto ga = arrange (text, spec);
        juce::Path p;
        ga.createPath (p);
        const int gaps = std::max (1, ga.getNumGlyphs() - 1);
        return (inkWidth - p.getBounds().getWidth()) / static_cast<float> (gaps);
    }

    void drawTextInInkBox (juce::Graphics& g, const juce::String& text, Weight weight, juce::Colour colour,
                           float inkLeft, float inkRight, float capTop, float capBottom)
    {
        const float capHeight = capBottom - capTop;
        TextSpec spec { weight, capHeight, 0.0f, colour };
        if (text.length() > 1)
            spec.tracking = trackingForInkWidth (text, weight, capHeight, inkRight - inkLeft);
        drawText (g, text, spec, 0.5f * (inkLeft + inkRight), capTop, juce::Justification::centred);
    }
}
