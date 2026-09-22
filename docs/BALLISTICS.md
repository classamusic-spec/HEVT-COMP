# Ballistics

`Source/DSP/Ballistics.*` — the gain-reduction envelope, in dB, double
precision.

```
target (dB, ≤ 0) from the gain computer
  │
  ├─ hold   r : instant towards deeper reduction; releases towards the target
  │             with the RELEASE time constant
  ├─ attack y1: follows r with the ATTACK time constant while r is deeper,
  │             follows r directly while it releases
  ├─ memory y2: charges towards depth·y1, releases slowly
  │             output = y1 + weight · min(0, y2 − y1)
  └─ rounding : optional second pole (WARM)
```

Coefficients are sample-rate independent: `k = 1 − exp(−1 / (τ · fs))`,
computed with `expm1` and applied in double precision. (A float
implementation was measured first: at 192 kHz a 3 s constant needs
k ≈ 1.7e-6, where float lost ~2 % accuracy. The double version agrees across
44.1 and 192 kHz to 0.01 ms on a 1.3 s release.)

## Why hold-then-attack

A pure branching detector lets the release branch pull the reduction back
between waveform peaks, so the effective attack on a sine is slower than
set (measured with that design: 3.16 ms set → 5.2 ms actual; 100 → 158 ms).
Holding the per-cycle peak first and applying the attack pole afterwards
makes both times exact and independent:

## Measured through the full compressor (1 kHz, −30 → −6 dBFS step, CLEAN)

| Panel | Attack set | Attack measured | Release set | Release measured |
|---|---|---|---|---|
| FAST (0 %) | 0.10 ms | 0.19 ms* | 20 ms | 20.0 ms |
| 25 % | 0.56 ms | 0.65 ms* | 70 ms | 70.0 ms |
| MID (50 %) | 3.16 ms | 3.23 ms | 245 ms | 244.9 ms |
| 75 % | 17.8 ms | 17.9 ms | 857 ms | 857.0 ms |
| SLOW (100 %) | 100 ms | 100.04 ms | 3000 ms | 2999 ms |

\* A 1 kHz peak detector cannot respond faster than the first half-cycle
reaches its peak (≈ 0.1 ms); the residual is the signal, not the ballistics.
The ballistics alone measure within 5 % or 1.5 samples at every rate from
44.1 to 192 kHz (`BallisticsTests`).

## Attack rounding (WARM)

A second pole of 0.4 ms + 0.35 × attack softens the onset: after 0.1 ms of a
10 dB step the sharp envelope is at −0.51 dB, the rounded one at −0.03 dB.

## Timing smoothing

Attack / release / memory times glide per block in the log-time domain
(50 ms), so turning ATTACK from 0.1 to 100 ms or switching modes never makes
the envelope jump.
