#pragma once

#include <vector>

namespace heat
{
    // One factory preset. Values are in user units (dB, %, ms, Hz) and are
    // written through the parameters' own ranges when loaded.
    struct FactoryPreset
    {
        const char* name;
        const char* category;
        const char* tags;          // "WARM · SMOOTH · PRESENT"
        float inputDb;
        float outputDb;
        float compress;            // %
        float attackMs;
        float releaseMs;
        int mode;                  // 0 CLEAN, 1 WARM, 2 DRIVE
        int detector;              // 0 PEAK, 1 RMS, 2 OPTICAL
        float tube;                // %
        float iron;                // %
        float mix;                 // %
        float hpfHz;
        int link = 0;              // 0 LINKED, 1 PARTIAL, 2 DUAL MONO

        // 2.1 — defaults are neutral, so every 2.0 preset loads unchanged
        // (apart from the IRON model, which is HYSTERESIS for factory presets).
        int stereoMode = 0;        // 0 L/R, 1 M/S, 2 MID, 3 SIDE
        int lookahead = 0;         // LOOKAHEAD choice index (0 = OFF)
        float scLpfHz = 20000.0f;  // 20 kHz = off
        float scEqHz = 3000.0f;
        float scEqDb = 0.0f;
        float scEqQ = 1.0f;
        bool limiter = false;
        float ceilingDb = -1.0f;
        int ironModel = 1;         // 0 CLASSIC, 1 HYSTERESIS
        int multiband = 0;         // 0 OFF, 1 2 BAND, 2 3 BAND
        float xoverLowHz = 150.0f;
        float xoverHighHz = 2500.0f;
        float bandLow = 100.0f, bandMid = 100.0f, bandHigh = 100.0f; // %
    };

    const std::vector<FactoryPreset>& getFactoryPresets();

    // Canonical category order for menus.
    const std::vector<const char*>& getPresetCategories();
}
