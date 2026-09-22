#pragma once

#include "DSP/ControlMappings.h"

// Stable, versioned parameter identifiers. These are persisted in DAW sessions
// and presets: NEVER rename or re-purpose an ID after release. New behaviour
// gets a new ID (".v3") and a migration path in PluginState.

namespace heat::ids
{
    inline constexpr const char* input          = "heat.input.v2";
    inline constexpr const char* output         = "heat.output.v2";
    inline constexpr const char* compress       = "heat.compress.v2";
    inline constexpr const char* attack         = "heat.attack.v2";
    inline constexpr const char* release        = "heat.release.v2";
    inline constexpr const char* mode           = "heat.mode.v2";
    inline constexpr const char* detector       = "heat.detector.v2";
    inline constexpr const char* tube           = "heat.tube.v2";
    inline constexpr const char* iron           = "heat.iron.v2";
    inline constexpr const char* mix            = "heat.mix.v2";
    inline constexpr const char* hpf            = "heat.sidechain.hpf.v2";
    inline constexpr const char* scSource       = "heat.sidechain.source.v2";
    inline constexpr const char* scLink         = "heat.sidechain.link.v2";
    inline constexpr const char* scListen       = "heat.sidechain.listen.v2";
    inline constexpr const char* autoMakeup     = "heat.autoMakeup.v2";
    inline constexpr const char* autoRelease    = "heat.autoRelease.v2";
    inline constexpr const char* quality        = "heat.quality.v2";
    inline constexpr const char* bypass         = "heat.bypass.v2";

    // --- 2.1 (appended; the ".v2" suffix names the parameter-set generation) ---
    inline constexpr const char* stereoMode     = "heat.stereo.mode.v2";
    inline constexpr const char* lookahead      = "heat.lookahead.v2";
    inline constexpr const char* scLpf          = "heat.sidechain.lpf.v2";
    inline constexpr const char* scEqFreq       = "heat.sidechain.eq.freq.v2";
    inline constexpr const char* scEqGain       = "heat.sidechain.eq.gain.v2";
    inline constexpr const char* scEqQ          = "heat.sidechain.eq.q.v2";
    inline constexpr const char* limiter        = "heat.limiter.v2";
    inline constexpr const char* ceiling        = "heat.limiter.ceiling.v2";
    inline constexpr const char* ironModel      = "heat.iron.model.v2";
    inline constexpr const char* multiband      = "heat.multiband.v2";
    inline constexpr const char* xoverLow       = "heat.multiband.xover.low.v2";
    inline constexpr const char* xoverHigh      = "heat.multiband.xover.high.v2";
    inline constexpr const char* bandLow        = "heat.multiband.low.v2";
    inline constexpr const char* bandMid        = "heat.multiband.mid.v2";
    inline constexpr const char* bandHigh       = "heat.multiband.high.v2";
}

namespace heat::ranges
{
    inline constexpr float gainMinDb     = -24.0f;
    inline constexpr float gainMaxDb     =  24.0f;
    inline constexpr float attackMinMs   = dsp::attackMinMs;
    inline constexpr float attackMaxMs   = dsp::attackMaxMs;
    inline constexpr float releaseMinMs  = dsp::releaseMinMs;
    inline constexpr float releaseMaxMs  = dsp::releaseMaxMs;
    inline constexpr float hpfMinHz      = dsp::hpfMinHz;
    inline constexpr float hpfMaxHz      = dsp::hpfMaxHz;

    inline constexpr float scLpfMinHz    = 1000.0f;
    inline constexpr float scLpfMaxHz    = 20000.0f;   // = off
    inline constexpr float scEqMinHz     = 80.0f;
    inline constexpr float scEqMaxHz     = 12000.0f;
    inline constexpr float scEqMaxDb     = 18.0f;
    inline constexpr float scEqMinQ      = 0.3f;
    inline constexpr float scEqMaxQ      = 8.0f;
    inline constexpr float ceilingMinDb  = -12.0f;
    inline constexpr float xoverLowMinHz = 40.0f;
    inline constexpr float xoverLowMaxHz = 1000.0f;
    inline constexpr float xoverHighMinHz = 1000.0f;
    inline constexpr float xoverHighMaxHz = 12000.0f;
    inline constexpr float bandAmountMax = 2.0f;       // 200 %

    // LOOKAHEAD choices (index → ms).
    inline constexpr float lookaheadMs[] = { 0.0f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f };
}

namespace heat
{
    inline constexpr const char* productName    = "HEAT";
    inline constexpr const char* productTagline = "ANALOG DYNAMICS PROCESSOR";
    inline constexpr const char* brandName      = "NOVA AUDIO";
    // 3 = HEAT 2.1: STEREO MODE, LOOKAHEAD, SC EQ, LIMITER, IRON MODEL,
    //     MULTIBAND. Older sessions and presets are migrated on load (their
    //     IRON keeps the CLASSIC model; new controls start neutral).
    inline constexpr int stateVersion           = 3;
}
