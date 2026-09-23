#pragma once

#include <juce_graphics/juce_graphics.h>

// Geometry of the locked HEAT reference, in reference pixels (the editor's
// design canvas is exactly the reference image: 1536 x 1024). Every value
// here was measured from design/reference/HEAT_LOCKED_REFERENCE.png; the
// editor scales the whole canvas uniformly, so proportions never drift.

namespace heat::ui::layout
{
    inline constexpr float canvasWidth = 1536.0f;
    inline constexpr float canvasHeight = 1024.0f;

    // --- Chassis ---------------------------------------------------------
    inline constexpr float chassisBottom = 1008.0f;
    inline constexpr float chassisRadius = 13.0f;
    inline constexpr float faceTop = 19.0f;
    inline constexpr float faceBottom = 931.0f;
    inline constexpr float faceInsetX = 1.0f;
    inline constexpr float faceRadius = 12.0f;

    // --- Header ----------------------------------------------------------
    inline constexpr float logoTop = 64.3f, logoBottom = 102.7f, logoStroke = 5.3f;
    inline constexpr float logoH0 = 55.0f, logoH1 = 90.7f;
    inline constexpr float logoE0 = 115.0f, logoE1 = 149.0f;
    inline constexpr float logoA0 = 170.0f, logoA1 = 210.0f;
    inline constexpr float logoT0 = 225.0f, logoT1 = 261.7f;
    inline constexpr float logoDividerX = 287.5f;

    inline const juce::Rectangle<float> presetOuter { 512.0f, 47.0f, 513.0f, 73.0f };
    inline const juce::Rectangle<float> presetPrev  { 516.5f, 51.5f, 57.0f, 64.0f };
    inline const juce::Rectangle<float> presetNext  { 964.5f, 51.5f, 57.0f, 64.0f };
    inline const juce::Point<float> heartCentre { 936.3f, 84.0f };
    inline const juce::Point<float> gearCentre  { 1136.7f, 83.7f };
    inline constexpr float headerDividerX = 1181.7f;
    inline constexpr float abBaseline = 103.3f;

    // --- Knobs -------------------------------------------------------------
    inline const juce::Point<float> inputCentre    { 187.5f, 370.0f };
    inline const juce::Point<float> outputCentre   { 1351.5f, 372.0f };
    inline const juce::Point<float> attackCentre   { 474.5f, 389.0f };
    inline const juce::Point<float> releaseCentre  { 1063.0f, 390.0f };
    inline const juce::Point<float> compressCentre { 769.5f, 713.0f };
    inline const juce::Point<float> tubeCentre     { 188.5f, 832.3f };
    inline const juce::Point<float> ironCentre     { 448.3f, 832.5f };
    inline const juce::Point<float> mixCentre      { 1143.0f, 832.5f };
    inline const juce::Point<float> hpfCentre      { 1414.5f, 832.5f };

    // --- Gain reduction meter ---------------------------------------------
    inline const juce::Rectangle<float> meterOuter { 639.0f, 149.0f, 261.0f, 395.0f };
    inline constexpr float meterOuterRadius = 38.0f;
    inline constexpr float meterRim = 9.0f; // machined bezel around the cover glass
    inline constexpr float meterColumnTop = 212.0f, meterColumnBottom = 522.0f;
    inline constexpr float meterLeftBarX0 = 680.0f, meterScaleX0 = 725.0f, meterScaleX1 = 811.0f, meterRightBarX1 = 857.5f;
    inline constexpr float meterDb[5]   { 0.0f, -3.0f, -6.0f, -12.0f, -24.0f };
    inline constexpr float meterDbY[5]  { 239.0f, 305.0f, 366.0f, 428.5f, 491.5f };

    // --- Selectors ----------------------------------------------------------
    inline constexpr float buttonTop = 648.5f, buttonHeight = 47.5f;
    inline constexpr float modeX0 = 78.3f, modeButtonW = 138.8f, modeGap = 13.5f;
    inline constexpr float detX0 = 1046.7f, detButtonW = 128.8f, detGap = 13.0f;

    // --- Separators ----------------------------------------------------------
    inline constexpr float headerLineY = 143.0f;
    inline constexpr float midGrooveY = 562.0f;
    inline constexpr float lowGrooveY = 749.0f;
    inline constexpr float bottomDividerX[4] { 283.0f, 560.0f, 983.0f, 1252.0f };
    inline constexpr float bottomDividerTop = 784.0f, bottomDividerBottom = 900.0f;

    // --- Knob drawing styles -------------------------------------------------
    struct KnobGeometry
    {
        float faceRadius;
        float skirtRadius;
        float pointerInner, pointerOuter, pointerWidth;
        float arcRadius;          // cyan / amber arc radius (0 = none)
        float trackInner;         // grey track ring (0 = none)
        float trackOuter;
        float tickRadius;         // decorative ticks (0 = none)
        float shadowOffset, shadowRadius;
    };

    inline constexpr KnobGeometry largeKnob    { 98.5f, 116.0f, 41.0f, 93.0f, 3.6f, 118.5f, 0.0f, 0.0f, 0.0f, 10.0f, 16.0f };
    inline constexpr KnobGeometry mediumKnob   { 53.8f, 63.0f, 18.0f, 52.0f, 3.2f, 67.8f, 64.5f, 71.0f, 0.0f, 7.0f, 12.0f };
    inline constexpr KnobGeometry compressKnob { 101.0f, 117.0f, 44.7f, 96.3f, 3.4f, 123.0f, 0.0f, 0.0f, 135.0f, 12.0f, 18.0f };
    inline constexpr KnobGeometry smallKnob    { 35.0f, 42.0f, 9.5f, 34.0f, 2.6f, 0.0f, 0.0f, 0.0f, 49.5f, 6.0f, 9.0f };
}
