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
    };

    const std::vector<FactoryPreset>& getFactoryPresets();

    // Canonical category order for menus.
    const std::vector<const char*>& getPresetCategories();
}
