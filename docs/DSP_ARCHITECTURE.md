# DSP Architecture

The whole engine lives in the JUCE-free `heat_dsp` library (`Source/DSP`,
`Source/Modes`, `Source/Nonlinear`, `Source/Quality`). The plugin
(`HeatAudioProcessor`) only reads parameters, forwards buffers and publishes
telemetry.

## Signal flow (as implemented in `HeatEngine`, 2.1)

```
 in ──► INPUT GAIN (20 ms) ─┬───────────────────────────────────────────────────► BYPASS line (total latency)
                            │
                            ├──► SIDECHAIN source (int / ext, 20 ms x-fade)
                            │      └► SC HPF ─► SC LPF ─► SC BELL (TPT SVFs, detector only)
                            │            ├► SC LISTEN line (L + lookahead)
                            │            └► [L/R → M/S rotation] ─► CompressorEngine (full band)
                            │                                     └► MULTIBAND: LR4 split ─► 2 / 3 band compressors
                            │                     → per channel: reference gain + LOW / HIGH shelf ratios, colour drive
                            │
                            └──► LOOKAHEAD line (0 … 10 ms, cross-faded taps)
                                   ├► DRY delay (L)
                                   └► [L/R → M/S rotation] ─► WET
                                        colour path (when TUBE / IRON / WARM / DRIVE active):
                                          ↑OS → TUBE → × gain (+ band shelves) → MODE COLOUR → IRON → pad → ↓OS
                                        otherwise:
                                          DRY × gain delayed by L (+ band shelves)
                                        MID / SIDE only: the unprocessed channel is the (delayed) dry channel

 [M/S → L/R] ─► MIX (linear) ─► SC LISTEN ─► OUTPUT GAIN ─► LIMITER (true peak, look-ahead line) ─► BYPASS x-fade ─► SAFETY ─► out
```

Latency = `L` (oversampler round trip, 79 samples ≤ 96 kHz, 69 above) +
LOOKAHEAD + limiter look-ahead (W − 1 + 24 samples, 95 at 48 kHz) when the
LIMITER is on. See `STEREO_AND_LOOKAHEAD.md`, `SIDECHAIN_EQ.md`,
`LIMITER.md`, `MULTIBAND.md`.

## Key decisions

**Constant core latency.** The colour path always runs through the
linear-phase oversampler, whose round trip is padded to an integer number of
base-rate samples. The ULTRA (8x) round trip sets the core latency `L` (79
samples at ≤ 96 kHz; 69 above 100 kHz where one 2x stage is dropped). NORMAL /
HIGH and the no-colour delay path are padded to the same `L`, so latency never
changes with TUBE, IRON, MODE, QUALITY, STEREO MODE or MULTIBAND.

**Latency that the user asks for (2.1).** LOOKAHEAD and the LIMITER add
latency on purpose. Every path whose alignment depends on it (look-ahead,
SC listen, limiter, bypass) is a `CrossfadeDelay`: a new length is reached
by a 20 ms smoothstep cross-fade between the old and new read taps, so the
change is click-free (measured click ratio 1.02 against the steady state).
The processor reports the new latency to the host from the message thread
(directly, or via an async update when a change arrives on another thread);
both controls are non-automatable.

**Colour path hand-over.** When no colour is active (CLEAN, TUBE = IRON = 0)
the wet signal is `dry[n] × gain[n − L]` — the oversampler is not run (0.4 %
CPU instead of 1–5 %). Engaging colour primes the oversampler for `2L + 64`
samples, then cross-fades over 10 ms; disengaging fades back. Both paths are
time-aligned so the fade is inaudible. Quality changes go through the same
fade-out / switch / prime / fade-in sequence. From a reset (silence) the
colour path starts engaged with no fade.

**Gain applied inside the oversampled domain.** The base-rate gain signal is
linearly interpolated to the high rate and delayed by the up-sampler latency
minus the interpolation lag, so gain lands on the exact samples it was
computed for. TUBE therefore sits *before* the gain (it is driven by INPUT,
not by the compressor) and IRON *after* (makeup drives it), as in the
product spec.

**Dry/wet alignment.** The dry path is delayed by `L`; with neutral settings
the output is a bit-exact delayed copy of the input at any MIX value
(measured residual 0.0). MIX is a linear blend because dry and wet are
phase-coherent (an equal-power law would bump +3 dB at 50 %).

**Distortion-only DC blocking.** Each nonlinear stage computes
`y = x + dcBlock(f(x) − x)`: only the distortion component is high-passed,
so the linear part of the wet signal keeps exactly the dry path's phase.

**Transitions without corners (2.1).** Every new fade (delay taps, M/S
morph, MID / SIDE only, multiband in / out, limiter enable) uses a smoothstep
curve (zero slope at both ends); a linear ramp leaves a slope kink that shows
up as a −64 dB transient in the second difference.

**Smoothing per parameter type.** INPUT / OUTPUT / MIX: 20 ms linear ramps.
COMPRESS curve outputs (threshold, slope, knee, makeup, colour drive/bias):
40 ms one-pole per sample. Detector weights and link: 25 ms linear. Timing
(attack/release/memory): 50 ms glide per block in the log-time domain. HPF:
30 ms log-frequency glide, coefficients refreshed every 16 samples. TUBE /
IRON: 30 ms one-pole.

**Safety.** Non-finite samples are replaced with 0 and the engine state is
reset at the end of that block; output is hard-limited at ±31.6 (≈ +30 dBFS),
which normal material never reaches. Denormals are flushed (FTZ/DAZ) inside
`HeatEngine::process` and `juce::ScopedNoDenormals` in the processor.

**Realtime safety.** Everything is allocated in `prepare()`. The engine
processes host blocks of any size in fixed 256-sample chunks, so no buffer
depends on the host's block size. Measured: 0 heap allocations across 3000
randomised audio callbacks (see `TESTING.md`). No locks, no logging, no
system calls on the audio thread.

## Telemetry

`HeatEngine` fills an `EngineTelemetry` per block. `MeterTelemetry` (atomics,
lock-free) accumulates the deepest reduction since the UI last read it, so no
peak is missed regardless of block size or frame rate.

## Files

| File | Role |
|---|---|
| `DSP/GainComputer.*` | Static curve |
| `DSP/PeakDetector.h`, `RMSDetector.h`, `OpticalDetector.h` | Level sensing |
| `DSP/Ballistics.*` | Gain-reduction envelope |
| `DSP/CompressMacro.*`, `Modes/ModeProfiles.*` | COMPRESS macro and mode / detector profiles |
| `DSP/CompressorEngine.*` | Detection → link → curve → ballistics |
| `DSP/SidechainFilter.*` | Detector HPF, LPF and bell |
| `DSP/DryWetAligner.h` | Delay lines, `CrossfadeDelay` (variable latency) |
| `DSP/Multiband.*` | TPT SVF, LR4 band splitter (detector), dynamic shelves (audio) |
| `DSP/TruePeakLimiter.*` | Output limiter |
| `DSP/HeatEngine.*` | Full chain, latency, stereo domain, multiband, colour path, mix, limiter, bypass, safety |
| `Nonlinear/TubeStage.*`, `IronStage.*`, `Hysteresis.h`, `ModeColor.h`, `DCBlocker.h`, `SaturationUtilities.h` | Colour |
| `Quality/Oversampler.*` | Polyphase half-band cascade |
