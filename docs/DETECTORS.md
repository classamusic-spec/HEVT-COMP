# Detectors

All three detectors run continuously on every channel; the DETECTOR
selection only changes the blend weights (25 ms linear cross-fade in the dB
domain), so switching is click-free (measured: second-difference peak equal
to the steady-state signal, `EngineTests`).

## PEAK

Instantaneous rectified level, `20·log10|x|`. No smoothing in the detector:
timing lives in the ballistics, which hold the per-cycle peak so the attack
time is accurate on periodic material.

## RMS — true power

```
power         = x²
smoothedPower = onePole(power, 12 ms)
rmsDb         = 10·log10(2 · smoothedPower)     (AES17: a sine reads its peak level)
```

This is a real power estimate, not an average of |x|. Measured on a 10 %
duty pulse train: reading −8.93 dB = true RMS −8.93 dB, versus −18.93 dB an
absolute-mean detector would report. Sine ripple after settling: 0.029 dB.
The AES17 reference keeps the COMPRESS macro calibrated when switching from
PEAK (a steady sine reads the same on both).

Transients read lower than on PEAK (2 ms full-scale noise burst: PEAK −0.04
dB, RMS −9.57 dB), which is what "energy-oriented, less transient sensitive"
means in practice.

## OPTICAL — original light-cell behaviour

Not a model of any specific optical compressor circuit.

1. *Emission*: signal power drives a light level with asymmetric response —
   1.5 ms rise, 18 ms fall (measured −3 dB rise 0.85 ms, fall 21.6 ms).
2. *Cell*: the ballistics are switched to the optical profile:
   * attack = max(2.5 × panel attack, 4 ms) — slow to react to moderate
     level changes…
   * …but it accelerates by `1 + 0.12 × excessDb` for large jumps (faster
     reaction to big energy changes);
   * fast stage release = 0.6 × panel release;
   * full program memory (weight 1.0, depth 0.6, charge 350 ms, slow stage
     8 × release) → multi-stage, history-dependent recovery.

Measured (release 150 ms, CLEAN): 90 % recovery after a 50 ms burst 1003 ms,
after 3 s of sustained compression 2289 ms; the same test on PEAK gives
346 / 347 ms (no memory). See `PROGRAM_DEPENDENCE.md`.

## Distinctness (drum-like programme, CLEAN 60 %, attack 0.3 ms)

| | PEAK | RMS | OPTICAL |
|---|---|---|---|
| deepest GR | −14.7 dB | −6.1 dB | −10.6 dB |
| mean GR | −5.3 dB | −2.4 dB | −4.9 dB |

GR-trace RMS distance: PEAK–RMS 3.60 dB, PEAK–OPTICAL 1.61 dB,
RMS–OPTICAL 2.75 dB (gate: > 1.5 dB for every pair).

## Stereo linking

Linked level = max(L, R) in dB; per channel `level += link · (linked −
level)` with link = 1 (LINKED), 0.5 (PARTIAL), 0 (DUAL MONO). Measured with a
hot left / quiet right signal: quiet-channel GR −16.54 (linked), −4.97
(partial), 0.00 dB (dual mono); linked L/R GR difference 1e-6 dB; worst
per-sample L/R gain mismatch through the full engine 5e-6 dB (no image
wander).

## Sidechain

Internal (post INPUT gain) or external (host sidechain bus; falls back to
internal when no bus is connected, 20 ms cross-fade). The detector path is
high-passed by a 2nd-order Butterworth TPT state-variable filter (20–400 Hz,
log-mapped, smoothed, modulation-stable). Measured: −3.01 dB at the cutoff,
−12.30 dB one octave below, 0.000 dB a decade above; the audible signal is
untouched (deviation from the delayed input 0.0 at every HPF setting). SC
LISTEN outputs the filtered detector signal (30 Hz with HPF 400: −45 dB).
