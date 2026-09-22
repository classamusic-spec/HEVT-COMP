#pragma once

#include <juce_graphics/juce_graphics.h>

// Vector icons and the HEAT logotype, in reference-canvas coordinates.
namespace heat::ui::icons
{
    // The HEAT logotype: H, E with a detached top bar, T (the Λ is separate).
    juce::Path logo();
    juce::Path logoLambda();                  // the Λ, drawn clipped to logoLambdaClip()
    juce::Rectangle<float> logoLambdaClip();

    juce::Path gear (juce::Point<float> centre, float size);
    juce::Path heart (juce::Point<float> centre, float width, float height);
    juce::Path chevron (juce::Point<float> centre, float width, float height, bool pointsLeft);
    juce::Path tubeWaves (juce::Rectangle<float> area);
    juce::Path ironCoils (juce::Rectangle<float> area);
    juce::Path crosshair (juce::Point<float> centre, float radius);
    juce::Path highPassCurve (juce::Rectangle<float> area);
}
