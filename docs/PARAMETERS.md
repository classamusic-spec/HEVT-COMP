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

Double-click resets to these defaults. Choice lists are append-only.

Text entry accepts plain numbers in the displayed unit (`"3.5"`, `"-6"`),
seconds for release (`"1.2 s"`), and percentages (`"35"` → 35 %).

## Session state

`getStateInformation` writes:

```xml
<HEAT_STATE version="2" presetName="…" uiScale="0.75" peakHold="1">
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

## Host program list

The 35 factory presets are also exposed as host programs
(`getNumPrograms` / `setCurrentProgram`).
