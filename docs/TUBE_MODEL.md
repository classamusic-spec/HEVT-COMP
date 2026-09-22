# TUBE Model

`Source/Nonlinear/TubeStage.*` — an original tube-inspired input colour
stage, running inside the oversampled wet path before the gain element.
It is not a circuit simulation of any specific tube stage.

## Transfer

```
v       = drive · x
b_dyn   = softFloor(bias − 0.25 · env, −0.1)        env = 40 ms follower of |v|
shaped  = [ s(v + b_dyn) − s(b_dyn) ] / (drive · s'(b_dyn))
s(u)    = u / sqrt(1 + u²)                          analytic algebraic sigmoid
y       = x + dcBlock(depth · (shaped − x))          distortion-only DC blocking
out     = y + hfBlend · (LP_18k(y) − y)              subtle HF smoothing
```

| amount | drive (log-spaced) | bias | depth | HF blend |
|---|---|---|---|---|
| 0 % | 0.10 | 0.04 | 0 (exact bypass) | 0 |
| 25 % | 0.22 | 0.105 | 1 | 0.15 |
| 50 % | 0.49 | 0.17 | 1 | 0.30 |
| 100 % | 2.40 | 0.30 | 1 | 0.60 |

* **Asymmetry from the operating point.** Running the symmetric sigmoid at
  bias `b` produces even harmonics proportional to `b`; drive sets how far
  into the curve the signal reaches (odd harmonics).
* **Level-dependent H2 / H3 balance.** At low level the bias dominates (H2);
  loud passages both reach further into the curve (H3) and pull the
  operating point towards the centre through the 40 ms bias shift (less H2)
  — a short memory effect.
* **Analytic curve.** A first version used a piecewise curve with different
  ceilings for the two half-waves; its 3rd derivative jumps at zero, so the
  harmonic series decays slowly. Measured aliasing (100 %, −3 dBFS, 10 kHz,
  4x) was −52 dBc. The analytic version measures −103 dBc under the same
  conditions.
* **Built-in gain compensation.** The small-signal gain is exactly 1 at
  every setting, so TUBE never wins by being louder.

## Measurements (4x oversampling, from `measurements/nonlinear_thd.csv`)

THD (%) of a 1 kHz sine:

| amount | −30 | −24 | −18 | −12 | −6 | 0 dBFS |
|---|---|---|---|---|---|---|
| 25 % | 0.055 | 0.110 | 0.214 | 0.410 | 0.753 | 1.297 |
| 50 % | 0.191 | 0.377 | 0.730 | 1.369 | 2.388 | 3.833 |
| 75 % | 0.563 | 1.097 | 2.077 | 3.661 | 5.612 | 9.473 |
| 100 % | 1.510 | 2.873 | 5.087 | 7.605 | 11.175 | 20.599 |

Harmonic balance at 50 % (dBc): −24 dBFS: H2 −48.5, H3 −80.0; −12 dBFS:
H2 −37.3, H3 −55.8; 0 dBFS: H2 −30.8, H3 −31.9 (H3 catches up with level).

| Check | Result |
|---|---|
| Subtle / warmer / saturated at −12 dBFS (25 / 50 / 100 %) | 0.41 / 1.37 / 7.6 % |
| THD rises with amount at every level and with level at every amount | PASS |
| DC offset (−3 dBFS, 50 % / 100 %) | −1.1e-5 / −7.3e-6 |
| Level change on −18 dBFS RMS programme (25/50/75/100 %) | −0.08 / −0.15 / −0.30 / −0.80 dB |
| Small-signal response at 100 % (−40 dBFS) | 100 Hz −0.00, 5 kHz −0.23, 10 kHz −0.80, 15 kHz −1.50, 20 kHz −2.17 dB |
| Exact bypass at 0 % | max deviation < 1e-7 |
| Finite and bounded for +18 dBFS inputs, 10 Hz … 20 kHz | PASS |

Aliasing: see `OVERSAMPLING.md`.

## Listening

`UNVERIFIED — ENVIRONMENT LIMITATION` (no audio device). Renders:
`wav/06_warm_tube50_iron50.wav`, `wav/07_drive_tube100_iron100.wav`.
