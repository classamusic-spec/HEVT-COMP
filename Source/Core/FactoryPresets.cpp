#include "Core/FactoryPresets.h"

namespace heat
{
    namespace
    {
        constexpr int CLEAN = 0, WARM = 1, DRIVE = 2;
        constexpr int PEAK = 0, RMS = 1, OPTICAL = 2;
    }

    const std::vector<FactoryPreset>& getFactoryPresets()
    {
        //  name              category      tags                                   in    out   comp  atk    rel    mode   det      tube  iron  mix  hpf
        static const std::vector<FactoryPreset> presets {
            { "Vocal Glue",      "ESSENTIALS", "WARM \xc2\xb7 SMOOTH \xc2\xb7 PRESENT",       0.0f, 0.0f, 50, 3.16f, 245,  WARM,  PEAK,    50, 50, 50,  89 },
            { "Init",            "ESSENTIALS", "NEUTRAL \xc2\xb7 START \xc2\xb7 BLANK",       0.0f, 0.0f, 50, 3.16f, 245,  WARM,  PEAK,     0,  0, 100, 20 },
            { "Gentle Glue",     "ESSENTIALS", "CLEAN \xc2\xb7 SUBTLE \xc2\xb7 GLUE",         0.0f, 0.0f, 25, 10.0f, 300,  CLEAN, RMS,      0,  0, 100, 60 },
            { "Smooth Control",  "ESSENTIALS", "SMOOTH \xc2\xb7 EVEN \xc2\xb7 MUSICAL",       0.0f, 0.0f, 45, 8.0f,  400,  WARM,  OPTICAL, 20,  0, 100, 80 },
            { "Punch",           "ESSENTIALS", "PUNCH \xc2\xb7 FAST \xc2\xb7 FORWARD",        0.0f, 0.0f, 45, 18.0f, 120,  DRIVE, PEAK,    15,  0, 100, 60 },

            { "Velvet Level",    "VOCALS",     "VELVET \xc2\xb7 LEVEL \xc2\xb7 WARM",         0.0f, 0.0f, 55, 6.0f,  500,  WARM,  OPTICAL, 30, 10, 100, 110 },
            { "Warm Forward",    "VOCALS",     "WARM \xc2\xb7 FORWARD \xc2\xb7 PRESENT",      0.0f, 1.0f, 55, 2.0f,  150,  WARM,  PEAK,    45, 20, 100, 120 },
            { "Punch Vocal",     "VOCALS",     "PUNCH \xc2\xb7 BITE \xc2\xb7 UPFRONT",        0.0f, 0.0f, 50, 4.0f,  90,   DRIVE, PEAK,    25,  0, 100, 130 },
            { "Silk Rider",      "VOCALS",     "CLEAN \xc2\xb7 SMOOTH \xc2\xb7 AIRY",         0.0f, 0.0f, 40, 12.0f, 350,  CLEAN, RMS,      0,  0, 100, 100 },
            { "Rap Clamp",       "VOCALS",     "TIGHT \xc2\xb7 FAST \xc2\xb7 DENSE",          0.0f, 0.0f, 70, 0.8f,  60,   DRIVE, PEAK,    20,  0, 100, 140 },

            { "Drum Clamp",      "DRUMS",      "CLAMP \xc2\xb7 FAST \xc2\xb7 AGGRESSIVE",     0.0f, 0.0f, 75, 0.3f,  80,   DRIVE, PEAK,    30, 20, 100, 40 },
            { "Punch Bus",       "DRUMS",      "PUNCH \xc2\xb7 SNAP \xc2\xb7 GLUE",           0.0f, 0.0f, 40, 20.0f, 150,  CLEAN, PEAK,     0,  0, 100, 90 },
            { "Driven Room",     "DRUMS",      "ROOM \xc2\xb7 SMASH \xc2\xb7 BIG",            4.0f, -2.0f, 90, 0.2f, 180,  DRIVE, PEAK,    60, 40, 100, 20 },
            { "Snare Crack",     "DRUMS",      "CRACK \xc2\xb7 SNAP \xc2\xb7 BRIGHT",         0.0f, 0.0f, 55, 12.0f, 70,   DRIVE, PEAK,    35,  0, 100, 120 },
            { "Kick Weight",     "DRUMS",      "WEIGHT \xc2\xb7 THUMP \xc2\xb7 ROUND",        0.0f, 0.0f, 40, 25.0f, 120,  WARM,  PEAK,     0, 60, 100, 20 },

            { "Bass Weight",     "BASS",       "WEIGHT \xc2\xb7 ROUND \xc2\xb7 SOLID",        0.0f, 0.0f, 50, 15.0f, 200,  WARM,  RMS,     20, 55, 100, 20 },
            { "Bass Hold",       "BASS",       "EVEN \xc2\xb7 HOLD \xc2\xb7 STEADY",          0.0f, 0.0f, 60, 10.0f, 350,  CLEAN, OPTICAL,  0, 30, 100, 20 },
            { "Growl Bass",      "BASS",       "GROWL \xc2\xb7 GRIT \xc2\xb7 DENSE",          0.0f, 0.0f, 65, 5.0f,  100,  DRIVE, PEAK,    55, 45, 100, 20 },

            { "Slow Glue",       "BUS",        "GLUE \xc2\xb7 SLOW \xc2\xb7 COHESIVE",        0.0f, 0.0f, 30, 30.0f, 600,  CLEAN, RMS,      0,  0, 100, 80 },
            { "Warm Bus",        "BUS",        "WARM \xc2\xb7 COHESIVE \xc2\xb7 ROUND",       0.0f, 0.0f, 35, 20.0f, 400,  WARM,  RMS,     25, 25, 100, 60 },
            { "Mix Bus Air",     "BUS",        "OPEN \xc2\xb7 GENTLE \xc2\xb7 CLEAR",         0.0f, 0.0f, 20, 30.0f, 250,  CLEAN, RMS,      0,  0, 100, 120 },
            { "Iron Mix",        "BUS",        "IRON \xc2\xb7 WEIGHT \xc2\xb7 DEPTH",         0.0f, 0.0f, 30, 25.0f, 350,  WARM,  RMS,     10, 70, 100, 60 },

            { "Master 1 dB",     "MASTER",     "TRANSPARENT \xc2\xb7 1 dB \xc2\xb7 GLUE",     0.0f, 0.0f, 22, 30.0f, 500,  CLEAN, RMS,      0,  0, 100, 60 },
            { "Master Gentle",   "MASTER",     "GENTLE \xc2\xb7 EVEN \xc2\xb7 OPEN",          0.0f, 0.0f, 30, 25.0f, 800,  CLEAN, OPTICAL,  0,  0, 100, 50 },
            { "Master Warmth",   "MASTER",     "WARM \xc2\xb7 SUBTLE \xc2\xb7 RICH",          0.0f, 0.0f, 25, 30.0f, 600,  WARM,  RMS,     15, 20, 100, 60 },

            { "Parallel Crush",  "PARALLEL",   "CRUSH \xc2\xb7 PARALLEL \xc2\xb7 ENERGY",     0.0f, 0.0f, 100, 0.2f, 60,   DRIVE, PEAK,    50, 30, 35, 20 },
            { "New York Lift",   "PARALLEL",   "LIFT \xc2\xb7 DENSE \xc2\xb7 PARALLEL",       0.0f, 0.0f, 85, 1.0f,  120,  DRIVE, PEAK,    30,  0, 45, 40 },
            { "Soft Parallel",   "PARALLEL",   "SOFT \xc2\xb7 FULL \xc2\xb7 PARALLEL",        0.0f, 0.0f, 80, 10.0f, 500,  WARM,  OPTICAL, 20,  0, 40, 60 },

            { "Tube Level",      "WARM",       "TUBE \xc2\xb7 LEVEL \xc2\xb7 RICH",           0.0f, 0.0f, 45, 8.0f,  450,  WARM,  OPTICAL, 60,  0, 100, 80 },
            { "Soft Bloom",      "WARM",       "BLOOM \xc2\xb7 SOFT \xc2\xb7 LUSH",           0.0f, 0.0f, 55, 40.0f, 700,  WARM,  RMS,     40, 25, 80, 60 },

            { "Heavy Motion",    "DRIVE",      "MOTION \xc2\xb7 HEAVY \xc2\xb7 PUMP",         0.0f, 0.0f, 80, 3.0f,  150,  DRIVE, RMS,     45, 30, 100, 40 },
            { "Overheated",      "DRIVE",      "OVERHEATED \xc2\xb7 EXTREME \xc2\xb7 FUZZ",   6.0f, -6.0f, 100, 0.1f, 40,  DRIVE, PEAK,   100, 80, 100, 20 },

            { "Pump Machine",    "CREATIVE",   "PUMP \xc2\xb7 RHYTHMIC \xc2\xb7 OBVIOUS",     0.0f, 0.0f, 95, 5.0f,  200,  DRIVE, PEAK,    10,  0, 100, 20 },
            { "Lo-Fi Iron",      "CREATIVE",   "LO-FI \xc2\xb7 DARK \xc2\xb7 IRON",           0.0f, -2.0f, 70, 15.0f, 300, DRIVE, RMS,     70, 100, 100, 60 },
            { "Breath Swell",    "CREATIVE",   "SWELL \xc2\xb7 SLOW \xc2\xb7 AMBIENT",        0.0f, 0.0f, 90, 50.0f, 1500, CLEAN, OPTICAL,  0,  0, 60, 80 },
        };

        // 2.1 presets: built from a base line plus the new controls.
        static const std::vector<FactoryPreset> all = []
        {
            auto list = presets;
            auto add = [&list] (FactoryPreset p, auto&& edit) { edit (p); list.push_back (p); };

            add ({ "De-Ess Vocal", "VOCALS", "DE-ESS \xc2\xb7 SMOOTH \xc2\xb7 CLEAR", 0.0f, 0.0f, 45, 0.5f, 60, CLEAN, PEAK, 0, 0, 100, 400 },
                 [] (FactoryPreset& p) { p.scEqHz = 6500.0f; p.scEqDb = 15.0f; p.scEqQ = 2.0f; });
            add ({ "Lookahead Clamp", "DRUMS", "CLAMP \xc2\xb7 LOOKAHEAD \xc2\xb7 TIGHT", 0.0f, 0.0f, 70, 0.5f, 90, DRIVE, PEAK, 20, 0, 100, 60 },
                 [] (FactoryPreset& p) { p.lookahead = 3; });
            add ({ "Mid Punch", "DRUMS", "MID \xc2\xb7 PUNCH \xc2\xb7 WIDE", 0.0f, 0.0f, 60, 12.0f, 120, DRIVE, PEAK, 25, 10, 100, 80 },
                 [] (FactoryPreset& p) { p.stereoMode = 2; });
            add ({ "M/S Width Glue", "BUS", "M/S \xc2\xb7 WIDE \xc2\xb7 GLUE", 0.0f, 0.0f, 35, 20.0f, 350, WARM, RMS, 15, 20, 100, 90, 2 },
                 [] (FactoryPreset& p) { p.stereoMode = 1; });
            add ({ "Multiband Master", "MASTER", "MULTIBAND \xc2\xb7 EVEN \xc2\xb7 LOUD", 0.0f, 0.0f, 30, 20.0f, 400, CLEAN, RMS, 0, 10, 100, 40 },
                 [] (FactoryPreset& p) { p.multiband = 2; p.xoverLowHz = 120.0f; p.xoverHighHz = 3500.0f;
                                         p.bandLow = 120.0f; p.bandMid = 80.0f; p.bandHigh = 90.0f;
                                         p.limiter = true; p.ceilingDb = -1.0f; });
            add ({ "Master Limit", "MASTER", "LIMIT \xc2\xb7 TRUE PEAK \xc2\xb7 SAFE", 0.0f, 0.0f, 20, 30.0f, 500, CLEAN, RMS, 0, 0, 100, 60 },
                 [] (FactoryPreset& p) { p.limiter = true; p.ceilingDb = -1.0f; });
            add ({ "Bass Tamer MB", "BASS", "MULTIBAND \xc2\xb7 TIGHT \xc2\xb7 CONTROL", 0.0f, 0.0f, 55, 10.0f, 200, WARM, RMS, 10, 40, 100, 20 },
                 [] (FactoryPreset& p) { p.multiband = 1; p.xoverLowHz = 110.0f; p.bandLow = 150.0f; p.bandHigh = 50.0f; });
            add ({ "Vintage Iron", "WARM", "IRON \xc2\xb7 CLASSIC \xc2\xb7 2.0", 0.0f, 0.0f, 35, 20.0f, 350, WARM, RMS, 10, 70, 100, 60 },
                 [] (FactoryPreset& p) { p.ironModel = 0; });
            return list;
        }();
        return all;
    }

    const std::vector<const char*>& getPresetCategories()
    {
        static const std::vector<const char*> categories {
            "ESSENTIALS", "VOCALS", "DRUMS", "BASS", "BUS", "MASTER", "PARALLEL", "WARM", "DRIVE", "CREATIVE"
        };
        return categories;
    }
}
