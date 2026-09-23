#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace heat::ui
{
    // The central dark-glass gain-reduction instrument.
    //
    // Two glowing columns (left / right channel) whose lit body starts at the
    // current reduction and burns hotter towards the base; the right column
    // carries a peak-hold needle. Scale: 0, -3, -6, -12, -24 dB.
    //
    // Ballistics (display only; the DSP value is never altered):
    //   attack  : instantaneous — a new maximum is shown on the next frame
    //   release : 60 ms time constant towards the measured value
    //   peak    : holds 1.5 s, then falls at 15 dB/s
    // The deepest value measured between two frames is always shown, so short
    // peaks are never skipped regardless of frame rate or block size.
    class GainReductionDisplay : public juce::Component
    {
    public:
        GainReductionDisplay();

        // Places the instrument (reference coordinates of the parent canvas).
        void setInstrumentBounds (juce::Rectangle<float> outer);

        // Feed from the UI clock: deepest reduction per channel since the
        // previous frame (dB, <= 0) and the elapsed time.
        void pushFrame (float deepestLeftDb, float deepestRightDb, double elapsedSeconds);

        // Shows a fixed value without ballistics (snapshots / previews).
        void setImmediate (float leftDb, float rightDb, float peakDb);

        void setPeakHoldEnabled (bool enabled);

        float getDisplayedDb (int channel) const noexcept { return shown[channel]; }
        float getPeakDb() const noexcept { return peak; }

        // GPU mode: the glass interior (back plate, embers, glow, needle) is
        // drawn by an OpenGL renderer underneath this component; paint() then
        // only draws the bezel around it and the scale and cover glass on top,
        // and each changed frame is announced through onGpuFrame instead of a
        // repaint.
        void setGpuMode (bool enabled);
        bool isGpuMode() const noexcept { return gpuMode; }
        std::function<void()> onGpuFrame;

        void paint (juce::Graphics& g) override;
        void resized() override;

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

        static float yForDb (float db); // reference-space y of a GR value
        static float heatFor (float db); // 0..1 burn intensity of a GR value

        // Shared with the GPU renderer and its verification (reference coordinates).
        static juce::Rectangle<float> glassBounds();
        static float glassRadius();
        // GPU mode: where the OpenGL layer shows through. It reaches a little
        // past the glass into the static bezel, so the seam between the two
        // layers falls on pixels both of them draw identically.
        static juce::Rectangle<float> gpuWindowBounds();
        static float gpuWindowRadius();
        static void paintBackground (juce::Graphics& g);
        // Scale column, ticks, legends and title (over the embers).
        static void paintOverlay (juce::Graphics& g);
        // The cover glass and the tube glass: reflections over everything else.
        static void paintGlass (juce::Graphics& g);
        // Everything that moves inside the glass: warmth, bloom, ember columns, rim glow.
        static void paintDynamic (juce::Graphics& g, float leftDb, float rightDb);
        // Peak-hold needle on the right column (drawn when peakDb < -0.05).
        static void paintNeedle (juce::Graphics& g, float peakDb);

    private:
        void renderLayers (float pixelScale);
        void repaintGlass();
        static void paintColumn (juce::Graphics& g, float x0, float x1, float db, bool outerEdgeLeft);

        juce::Rectangle<float> outerBounds;
        juce::Image background, overlay;
        float cachedScale = 0.0f;

        float shown[2] { 0.0f, 0.0f };
        float peak = 0.0f;
        bool gpuMode = false;
        double peakHoldTime = 0.0;
        bool peakHold = true;
    };
}
