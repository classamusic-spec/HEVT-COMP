# Parameters

IDs are defined once in `Source/Core/Constants.h` and **must never be
renamed or re-purposed** after release (`StateTests` guards the list). All
parameters use JUCE parameter version hint 1.

| ID | Name | Type | Range / choices | Default | In presets |
|---|---|---|---|---|---|
| `heat.input.v2` | Input | float | −24 … +24 dB | 0 dB | yes |
| `heat.output.v2` | Output | float | −24 … +24 dB | 0 dB | yes |
| `heat.compress.v2` | Compress | float | 0 … 100 % | 50 % | yes |
| `heat.attack.v2` | Attack | float (log) | 0.1 … 100 ms | 3.16 ms | yes |
| `heat.release.v2` | Release | float (log) | 20 … 3000 ms | 245 ms | yes |
| `heat.mode.v2` | Mode | choice | CLEAN, WARM, DRIVE | WARM | yes |
| `heat.detector.v2` | Detector | choice | PEAK, RMS, OPTICAL | PEAK | yes |
| `heat.tube.v2` | Tube | float | 0 … 100 % | 0 % | yes |
| `heat.iron.v2` | Iron | float | 0 … 100 % | 0 % | yes |
| `heat.mix.v2` | Mix | float | 0 … 100 % | 100 % | yes |
| `heat.sidechain.hpf.v2` | SC HPF | float (log) | 20 … 400 Hz | 20 Hz | yes |
| `heat.sidechain.source.v2` | SC Source | choice | INTERNAL, EXTERNAL | INTERNAL | no |
| `heat.sidechain.link.v2` | Stereo Link | choice | LINKED, PARTIAL, DUAL MONO | LINKED | yes |
| `heat.sidechain.listen.v2` | SC Listen | bool | — | off | no |
| `heat.autoMakeup.v2` | Auto Makeup | bool | — | on | yes |
| `heat.autoRelease.v2` | Auto Release | bool | — | off | yes |
| `heat.quality.v2` | Quality | choice | NORMAL (2x), HIGH (4x), ULTRA (8x) | HIGH | no |
| `heat.bypass.v2` | Bypass | bool | — | off (host bypass parameter) | no |
| **2.1 (appended)** | | | | | |
| `heat.stereo.mode.v2` | Stereo Mode | choice | L/R, M/S, MID, SIDE | L/R | yes |
| `heat.lookahead.v2` | Lookahead | choice, **not automatable** | OFF, 0.5, 1, 2, 5, 10 ms | OFF | yes |
| `heat.sidechain.lpf.v2` | SC LPF | float (log) | 1 … 20 kHz (20 kHz = off) | 20 kHz | yes |
| `heat.sidechain.eq.freq.v2` | SC EQ Freq | float (log) | 80 Hz … 12 kHz | 3 kHz | yes |
| `heat.sidechain.eq.gain.v2` | SC EQ Gain | float | −18 … +18 dB | 0 dB | yes |
| `heat.sidechain.eq.q.v2` | SC EQ Q | float (log) | 0.3 … 8 | 1.0 | yes |
| `heat.limiter.v2` | Limiter | bool, **not automatable** | — | off | yes |
| `heat.limiter.ceiling.v2` | Ceiling | float | −12 … 0 dBTP | −1.0 dBTP | yes |
| `heat.iron.model.v2` | Iron Model | choice | CLASSIC, HYSTERESIS | HYSTERESIS (2.0 sessions: CLASSIC) | yes |
| `heat.multiband.v2` | Multiband | choice | OFF, 2 BAND, 3 BAND | OFF | yes |
| `heat.multiband.xover.low.v2` | Low Crossover | float (log) | 40 Hz … 1 kHz | 150 Hz | yes |
| `heat.multiband.xover.high.v2` | High Crossover | float (log) | 1 … 12 kHz | 2.5 kHz | yes |
| `heat.multiband.low.v2` / `.mid.v2` / `.high.v2` | Low / Mid / High Band | float | 0 … 200 % | 100 % | yes |

Double-click resets to these defaults. Choice lists are append-only, and so
is the parameter list: the 2.1 parameters come after the 2.0 ones, so host
parameter indices of 2.0 are unchanged (`StateTests` checks the order).

LOOKAHEAD and LIMITER change the plug-in latency. They are not automatable,
and every change is reported to the host from the message thread
(`HeatAudioProcessor::parameterChanged` → `setLatencySamples`, or an async
update when a change arrives on another thread).

Text entry accepts plain numbers in the displayed unit (`"3.5"`, `"-6"`),
seconds for release (`"1.2 s"`), and percentages (`"35"` → 35 %).

## Session state

`getStateInformation` writes:

```xml
<HEAT_STATE version="3" presetName="…" uiScale="0.75" peakHold="1" gpuMeter="1">
  <HEAT> …APVTS parameters… </HEAT>
  <AB active="0|1">
    <SLOT index="0" presetName="…"><HEAT …/></SLOT>
    <SLOT index="1" presetName="…"><HEAT …/></SLOT>
  </AB>
</HEAT_STATE>
```

A bare `<HEAT>` tree (pre-release format) is still accepted; unknown or
corrupt data is ignored. Verified by `StateTests` (20 randomised round
trips, A/B persistence, legacy and garbage input).

### Migration (version 2 → 3)

`HeatAudioProcessor::migrateParameterTree` runs on the session tree, on
both A/B slots and on legacy bare trees. A 2.0 tree has no IRON MODEL entry;
it gets `CLASSIC`, so a 2.0 session sounds exactly as it did (APVTS would
otherwise give it the new default, HYSTERESIS). All other 2.1 parameters are
missing from 2.0 trees and load at their neutral defaults. User presets
(`HEAT_PRESET version="2"`) are treated the same way: 2.1 controls the preset
does not mention are reset to neutral instead of keeping whatever was set
before, and IRON stays CLASSIC. Verified by `StateTests` ("2.0 sessions and
presets keep the CLASSIC IRON; new controls start neutral").

## Host program list

The 43 factory presets are also exposed as host programs
(`getNumPrograms` / `setCurrentProgram`).
