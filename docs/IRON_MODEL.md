# IRON Model

`Source/Nonlinear/IronStage.*` — transformer-inspired output weight,
running oversampled after the gain element and mode colour (so makeup gain
drives it). It is **not** a physical transformer model; it borrows the
mechanism by which transformers distort — core flux is the time integral of
the voltage — so the colouring is naturally low-frequency dependent.

```
φ      = leaky ∫ x dt        normalised so |φ| ≈ |x| at 50 Hz, leak 8 Hz (bounded)
φ_h    = φ − hc · sigmoid(dφ/dt)      direction-dependent offset (hysteresis-like memory)
e      = −k · φ_h³ / (1 + φ_h²)       soft cubic → mostly odd harmonics
weight = x + (G − 1) · LP1_80Hz(x)    additive first-order low shelf, G = +1.6 dB · amount
y      = weight + dcBlock(depth · e)
out    = y + hfBlend · (LP_24k(y) − y)
```

`k = 0.5·a² + 0.15·a`, `hc = 0.04·a`, `hfBlend = 0.5·a`; amount 0 is an exact
bypass.

A 2nd-order shelf was tried first; its phase produced a −0.27 dB dip at
200 Hz. The first-order shelf has no dip and extends gently into the low
mids (low-mid reinforcement).

## Measurements (4x, `measurements/nonlinear_thd.csv`)

THD (%) at −12 dBFS:

| amount | 25 % | 50 % | 75 % | 100 % |
|---|---|---|---|---|
| 50 Hz | 0.095 | 0.270 | 0.521 | 0.847 |
| 1 kHz | 0.0000 | 0.0002 | 0.0007 | 0.0021 |

At 100 % and 0 dBFS the 50 Hz THD is 5.97 %; at 1 kHz 0.011 % — the
colouring is ~400× stronger in the bass.

Small-signal response at 100 % (−40 dBFS): 20 Hz +1.52, 40 Hz +1.32,
90 Hz +0.78, 200 Hz +0.26, 1 kHz +0.01, 10 kHz −0.39, 15 kHz −0.77 dB.

| Check | Result |
|---|---|
| LF-dependent saturation (50 Hz THD > 10 × 1 kHz THD) | PASS (≈ 400×) |
| THD rises with amount | PASS |
| DC offset | 0.0 (distortion component DC-blocked) |
| Level change on −18 dBFS RMS programme | −0.02 … −0.03 dB |
| Exact bypass at 0 % | PASS |
| Finite and bounded for +18 dBFS inputs, 10 Hz … 20 kHz | PASS |

## Listening

`UNVERIFIED — ENVIRONMENT LIMITATION`. Renders: `wav/06_…`, `wav/07_…`.
