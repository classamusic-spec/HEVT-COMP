#pragma once

#include <juce_graphics/juce_graphics.h>

namespace heat::ui::surface
{
    // Paints everything static on the HEAT front panel — chassis, satin face,
    // lip, grooves, legends, logotype and icons — in reference coordinates.
    void paintChassis (juce::Graphics& g);

    // Spun-aluminium knob face (conic reflection + fine concentric grain),
    // rendered at `diameterPx` physical pixels. Cheap to regenerate; callers
    // cache the result per size.
    struct SpunFaceStyle
    {
        float base = 168.0f;        // mean brightness (0..255)
        float conic = 45.0f;        // cos(2θ) reflection depth
        float tilt = 9.0f;          // top-vs-bottom asymmetry (light from above)
        float grain = 5.0f;         // concentric grain amplitude
        uint32_t seed = 1;
    };
    juce::Image makeSpunFace (int diameterPx, const SpunFaceStyle& style);

    // Fine satin/brushed noise tile (tileable, ARGB, alpha-modulated).
    const juce::Image& satinNoiseTile();

    // Horizontal groove with fading ends: dark line + highlight below.
    void drawGroove (juce::Graphics& g, float x0, float x1, float y, float fadeLeft, float fadeRight,
                     float darkAlpha, float lightAlpha);
}
