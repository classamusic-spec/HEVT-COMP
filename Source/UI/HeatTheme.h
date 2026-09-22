#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace heat::ui
{
    // ------------------------------------------------------------------
    // Locked HEAT palette (sampled from design/reference/HEAT_LOCKED_REFERENCE.png)
    // ------------------------------------------------------------------
    namespace colours
    {
        inline const juce::Colour ink          { 0xff1d2125 };  // logo, primary labels
        inline const juce::Colour label        { 0xff25292d };
        inline const juce::Colour labelSoft    { 0xff3a3e43 };  // scale legends
        inline const juce::Colour labelMuted   { 0xff4a4f54 };  // subtitle / footer
        inline const juce::Colour labelFaint   { 0xff83868a };  // inactive "B"
        inline const juce::Colour groove       { 0xff4b4f54 };
        inline const juce::Colour grooveLight  { 0xfff2f4f6 };
        inline const juce::Colour etch         { 0xffa4a8ad };
        inline const juce::Colour glassDark    { 0xff111315 };
        inline const juce::Colour glassMid     { 0xff1c1f22 };
        inline const juce::Colour amber        { 0xffff8b3d };
        inline const juce::Colour amberDeep    { 0xffd95f1c };
        inline const juce::Colour amberHot     { 0xffffd7b0 };
        inline const juce::Colour amberText    { 0xfff8e9dc };
        inline const juce::Colour cyan         { 0xff8fd4ff };
        inline const juce::Colour cyanHot      { 0xffe6f7ff };
        inline const juce::Colour presetName   { 0xffd3ecf7 };
        inline const juce::Colour presetTags   { 0xff9ba2a8 };
        inline const juce::Colour meterLegend  { 0xffe9e0d5 };
        inline const juce::Colour meterTitle   { 0xffb8b3ad };
        inline const juce::Colour pointer      { 0xff2e3135 };
    }

    enum class Weight { light, regular, medium, semibold };

    // Embedded Inter (SIL OFL) typefaces.
    juce::Typeface::Ptr typeface (Weight w);

    // Font whose capital letters are exactly capHeight px tall.
    juce::Font fontForCapHeight (Weight w, float capHeight);

    // ------------------------------------------------------------------
    // Tracked text. Positions are ink-accurate: the baseline sits at
    // capTop + capHeight and horizontal placement uses the glyph outlines,
    // so text lands on the same pixels as in the locked reference.
    // ------------------------------------------------------------------
    struct TextSpec
    {
        Weight weight = Weight::medium;
        float capHeight = 11.0f;
        float tracking = 0.0f;           // extra space between glyphs, px
        juce::Colour colour = colours::label;
    };

    // Outline of `text` laid out at the origin (baseline y = 0).
    juce::Path textPath (const juce::String& text, const TextSpec& spec);

    // Draws text so its ink is horizontally anchored by `justification`
    // (left / centred / right) at x, with capitals spanning capTop..capTop+capHeight.
    void drawText (juce::Graphics& g, const juce::String& text, const TextSpec& spec,
                   float x, float capTop, juce::Justification justification = juce::Justification::centred);

    // Draws text so its ink spans exactly [inkLeft, inkRight] horizontally and
    // its capitals span [capTop, capBottom]. Tracking is solved for.
    void drawTextInInkBox (juce::Graphics& g, const juce::String& text, Weight weight, juce::Colour colour,
                           float inkLeft, float inkRight, float capTop, float capBottom);

    // Tracking (px) that makes `text` span `inkWidth` at the given cap height.
    float trackingForInkWidth (const juce::String& text, Weight weight, float capHeight, float inkWidth);

    // Knob rotary range used throughout (radians either side of 12 o'clock).
    inline constexpr float rotaryHalfRange = 2.1380f; // 122.5 degrees
}
