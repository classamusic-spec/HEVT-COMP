# HEAT V2 — Product Specification

## Concept

HEAT makes sophisticated compression feel effortless. The panel is simpler
than the DSP: one macro (COMPRESS) drives a curated set of hidden expert
parameters, three MODES change the actual engine behaviour, three
DETECTORS change how level is sensed, and two colour controls (TUBE, IRON)
add optional analog character that is level compensated so it never wins by
being louder.

The intended workflow, all on one screen:

```
INPUT → COMPRESS → ATTACK → RELEASE → MODE → (TUBE / IRON) → MIX → OUTPUT
```

## Front panel

| Control | Range | Mapping | Notes |
|---|---|---|---|
| INPUT | −24 … +24 dB | linear dB | Drives the detector and the colour stages |
| OUTPUT | −24 … +24 dB | linear dB | Final gain, 20 ms smoothing |
| COMPRESS | 0 … 100 % | mode-specific macro | See `COMPRESS_MACRO.md` |
| ATTACK | 0.1 … 100 ms | logarithmic (noon ≈ 3.2 ms) | Panel legend FAST / SLOW |
| RELEASE | 20 … 3000 ms | logarithmic (noon ≈ 245 ms) | Panel legend FAST / SLOW |
| MODE | CLEAN / WARM / DRIVE | — | Real DSP differences (curves, timing, program dependence, colour) |
| DETECTOR | PEAK / RMS / OPTICAL | — | Level sensing and cell behaviour |
| TUBE | 0 … 100 % | — | Oversampled tube-inspired input colour |
| IRON | 0 … 100 % | — | Oversampled transformer-inspired output weight |
| MIX | 0 … 100 % (DRY → WET) | linear | Latency-aligned parallel compression |
| HPF | 20 … 400 Hz | logarithmic (noon ≈ 89 Hz) | Detector only, never the audible signal |

## Advanced panel (gear)

SIDECHAIN (internal / external), SC LISTEN, STEREO LINK (linked / partial /
dual mono), AUTO MAKEUP, AUTO RELEASE, QUALITY (NORMAL 2x / HIGH 4x / ULTRA
8x), METER HOLD, UI SIZE (75 / 100 / 125 %). Latency and version readout.

## Header

Preset browser (prev / next / categorised menu / favourites / save / delete),
settings gear, A / B comparison (right-click: copy A→B, B→A), brand mark.

## Presets

35 factory presets in 10 categories (ESSENTIALS, VOCALS, DRUMS, BASS, BUS,
MASTER, PARALLEL, WARM, DRIVE, CREATIVE) plus user presets (`*.heatpreset`
XML in the user application-data folder) and persistent favourites. A newly
inserted instance opens on **Vocal Glue**, whose controls all sit at
12 o'clock exactly like the locked reference.

## Formats and platforms

VST3 and Standalone on Windows / macOS / Linux; AU on macOS. Mono and stereo
main buses; optional mono / stereo sidechain bus. Constant latency (79
samples up to 96 kHz, 69 above 100 kHz) reported to the host.

## Non-goals for V2

M/S, lookahead, multiband, sidechain EQ, limiter stage (see the release report
for the V2.1 / V3 list).
