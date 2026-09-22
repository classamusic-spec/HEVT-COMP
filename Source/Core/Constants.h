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
}

namespace heat
{
    inline constexpr const char* productName    = "HEAT";
    inline constexpr const char* productTagline = "ANALOG DYNAMICS PROCESSOR";
    inline constexpr const char* brandName      = "NOVA AUDIO";
    inline constexpr int stateVersion           = 2;
}
