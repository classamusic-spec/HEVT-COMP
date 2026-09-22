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

## CPU (one stereo instance, 48 kHz, 256-sample blocks, Release, this machine)

| Configuration | NORMAL (2x) | HIGH (4x) | ULTRA (8x) |
|---|---|---|---|
| CLEAN, no colour (oversampler bypassed) | 0.41 % | 0.41 % | 0.41 % |
| CLEAN + TUBE 50 % | 1.30 % | 2.03 % | 3.13 % |
| WARM (mode colour) | 1.13 % | 1.53 % | 2.30 % |
| WARM + TUBE + IRON 50 % | 1.86 % | 2.72 % | 4.55 % |
| DRIVE + TUBE + IRON 50 % | 1.92 % | 3.16 % | 5.03 % |

Percent of one core (x86-64 container vCPU). Full table: `measurements/cpu.csv`.

## Gate result

**ANALOG: PASS** for every measurable requirement.

Listening: `UNVERIFIED — ENVIRONMENT LIMITATION` (no audio device). WAV
renders for audition: `heat_measure <dir>` → `wav/06_warm_tube50_iron50.wav`,
`wav/07_drive_tube100_iron100.wav`, `wav/08_parallel_drive_mix40.wav`.
