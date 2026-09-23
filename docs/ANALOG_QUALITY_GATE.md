# Analog Quality Gate

TUBE, IRON and the mode colour were measured independently with the same
up → stage → down path the engine uses (`Tests/StageHarness.*`,
`Tests/NonlinearTests.cpp`, `Tests/AliasingTests.cpp`,
`Tools/HeatMeasure.cpp`). Raw data: `docs/measurements/*.csv`.

| Requirement | TUBE | IRON |
|---|---|---|
| THD vs input level | **PASS** — rises with level and amount; −12 dBFS: 0.41 / 1.37 / 3.66 / 7.61 % at 25/50/75/100 % | **PASS** — strongly LF dependent: 50 Hz −12 dBFS 0.10 / 0.27 / 0.52 / 0.85 %; 1 kHz ≤ 0.002 % |
| Harmonic distribution | **PASS** — H2-dominant at low level (50 %, −24 dBFS: H2 −48.5, H3 −80.0 dBc); H3 catches up at 0 dBFS (−30.8 / −31.9 dBc) | **PASS** — odd-dominant (cubic core) with direction-dependent memory |
| Aliasing | **PASS** — 100 %, −3 dBFS, 10 kHz: −16.9 (1x) → −51.1 (2x) → −103.2 (4x) → −122.0 dBc (8x); typical use (50 %, −12 dBFS) ≤ −133 dBc from 2x | **PASS** — ≤ −105 dBc even at 1x above 1 kHz; ≤ −126 dBc at 4x |
| DC | **PASS** — ≤ 1.1e-5 (distortion-only DC blocking) | **PASS** — 0.0 |
| Frequency response vs level | Small signal (−40 dBFS, 100 %): flat to 1 kHz, −0.8 dB @ 10 kHz, −1.5 dB @ 15 kHz (intentional HF smoothing); level dependence captured by the THD / harmonic tables | Small signal (100 %): +1.52 dB @ 20 Hz, +1.32 @ 40 Hz, +0.26 @ 200 Hz, flat at 1 kHz, −0.77 @ 15 kHz |
| Gain compensation | **PASS** — small-signal gain exactly 1; −18 dBFS RMS programme changes by −0.08 … −0.80 dB | **PASS** — −0.02 … −0.03 dB |
| CPU | See below | See below |
| Exact bypass at 0 | **PASS** | **PASS** |
| Extreme input stability (+18 dBFS, 10 Hz … 20 kHz, 8x) | **PASS** | **PASS** |

## Mode colour

At a −12 dBFS 187.5 Hz tone with COMPRESS 60 % (compressor active):
CLEAN 0.026 % THD (pure gain-modulation distortion), WARM 0.52 %, DRIVE 4.3 %.
Mode switching is click-free.

## IRON HYSTERESIS (2.1 default)

The Jiles–Atherton core (`IRON_MODEL.md`) was put through the same gate:

| Requirement | Result |
|---|---|
| THD vs level | **PASS** — Rayleigh region at low level (×2.1 per +10 dB), saturation knee above (×3.5 from −12 to 0 dBFS); −12 dBFS, 50 Hz: 0.070 / 0.179 / 0.369 / 0.755 % at 25/50/75/100 % (CLASSIC 0.095 / 0.270 / 0.521 / 0.847) |
| Harmonic distribution | **PASS** — odd only on symmetric signals (H2 −170 dBc); even harmonics appear only from remanence (history), as in real iron |
| Hysteresis | **PASS** — symmetric loop, coercive field ±0.015, remanence 0.165, probe difference after ± history −60.5 dB (CLASSIC −85 dB) |
| Aliasing | **PASS** — 100 %, −3 dBFS, 10 kHz: −103.1 (1x) → −123.2 (2x) → −133.9 (4x) → −154.6 dBc (8x) |
| DC / level | **PASS** — DC 0.0; programme level +0.07 dB at 100 % |
| Exact bypass at 0, extreme inputs, click-free model switch | **PASS** |
| 2.0 compatibility | **PASS** — 2.0 sessions, A/B slots and presets load with CLASSIC |

## CPU (one stereo instance, 48 kHz, 256-sample blocks, Release, this machine)

2.0 (IRON = CLASSIC):

| Configuration | NORMAL (2x) | HIGH (4x) | ULTRA (8x) |
|---|---|---|---|
| CLEAN, no colour (oversampler bypassed) | 0.41 % | 0.41 % | 0.41 % |
| CLEAN + TUBE 50 % | 1.30 % | 2.03 % | 3.13 % |
| WARM (mode colour) | 1.13 % | 1.53 % | 2.30 % |
| WARM + TUBE + IRON 50 % | 1.86 % | 2.72 % | 4.55 % |
| DRIVE + TUBE + IRON 50 % | 1.92 % | 3.16 % | 5.03 % |

2.1 (IRON = HYSTERESIS, `measurements/cpu.csv`): WARM + TUBE + IRON 50 %
2.1 / 3.6 / 6.2 %, DRIVE + TUBE + IRON 50 % 2.1 / 3.9 / 5.9 % at NORMAL /
HIGH / ULTRA. The hysteresis core costs 34 ns per oversampled sample and
channel after optimisation (Euler step, float Langevin; the first version
cost 112 ns). Per 2.1 feature over a WARM / HIGH baseline
(`measurements/cpu_features.csv`, range over three runs): IRON HYSTERESIS
+0.9 … 1.4 % (CLASSIC +0.7 … 0.9 %), LIMITER +0.0 … 0.4 %, MULTIBAND 2 / 3 bands
+0.8 … 1.2 / +1.3 … 2.4 %, M/S, LOOKAHEAD and SC EQ within the noise;
everything at once 5.6 … 6.7 % (HIGH), 8.0 … 10.1 % (ULTRA).

Percent of one core (x86-64 container vCPU, shared; run-to-run variation
about ±20 %).

## Gate result

**ANALOG: PASS** for every measurable requirement, for both IRON models.

Listening: `UNVERIFIED — ENVIRONMENT LIMITATION` (no audio device). WAV
renders for audition: `heat_measure <dir>` → `wav/06_warm_tube50_iron50.wav`,
`wav/07_drive_tube100_iron100.wav`, `wav/08_parallel_drive_mix40.wav`,
`wav/12_iron100_hysteresis.wav` vs `wav/13_iron100_classic.wav`.
