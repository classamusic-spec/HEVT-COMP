# IRON Model

`Source/Nonlinear/IronStage.*` — output transformer colour, running
oversampled after the gain element and mode colour (so makeup gain drives
it). Both models share the mechanism by which transformers distort: the core
flux is the time integral of the voltage, so the colouring is naturally
low-frequency dependent, and the distortion is the voltage that the
nonlinear magnetising current drops across the source impedance.

```
φ      = leaky ∫ x dt        normalised so |φ| ≈ |x| at 50 Hz, leak 8 Hz (bounded)
e      = model (below)       distortion component, flux units
weight = x + (G − 1) · LP1_80Hz(x)    additive first-order low shelf, G = +1.6 dB · amount
y      = weight + dcBlock(e)
out    = y + hfBlend · (LP_24k(y) − y)          hfBlend = 0.5 · amount
```

IRON MODEL (advanced panel, GENERAL page) selects the model. **HYSTERESIS**
is the default from 2.1; sessions, A/B slots and user presets saved by 2.0
load as **CLASSIC** so they sound exactly as they did. Switching models
cross-fades the two distortion components over 20 ms (click-free, measured
second-difference ratio 1.0).

## HYSTERESIS (2.1) — Jiles–Atherton core

`Source/Nonlinear/Hysteresis.h`. A transformer driven from a voltage source
sets the flux density B; the field H the source must supply is a nonlinear,
history-dependent function of B. The inverse Jiles–Atherton model
(Sadowski et al., IEEE Trans. Magn. 2002) integrates the magnetisation M
along the B trajectory (normalised units, μ0 = 1):

```
He    = H + αM = B − (1 − α) M
Man   = Ms · L(He / a)                    L(x) = coth x − 1/x  (Langevin)
χ     = c · dMan/dHe + max(0, δ (Man − M)) / k      δ = sign(dB)
dM/dB = χ / (1 + (1 − α) χ)               H = B − M
```

The `max(0, ·)` term is the irreversible (domain-wall pinning) contribution
with M_irr eliminated through `M = c·Man + (1 − c)·M_irr`; it only pulls M
towards the anhysteretic curve, which removes the non-physical negative
susceptibility of the original formulation. χ ≥ 0 and α·χ < 1 keep every
branch monotone (dH/dB > 0), so the integration is unconditionally stable.

In the stage:

```
B = drive · φ,          drive = 1 + 3 · amount
H = JA(B)
e = −depth · (μi · H − B) / drive,   depth = 0.055 · amount
```

`μi` is the virgin small-signal permeability, so `μi·H − B` is the
magnetising current minus its linear (inductive) part — the linear part
would only be a low-frequency roll-off, which IRON does not want.

Parameters: Ms = 1, a = 0.03, k = 0.03, c = 0.4, α = 0.002 — a
high-permeability core (μi ≈ 5.4) with a narrow loop. They were chosen with a
calibration sweep so that the reference level (50 Hz, −12 dBFS) lands where
the CLASSIC model sits at every amount, while the level dependence follows
the physics:

* **Rayleigh region** (low level): distortion falls only in proportion to
  level (THD ×2.1 per +10 dB), where a polynomial core falls with the square
  of level or faster — real iron keeps a little grain at low level.
* **Saturation knee** (high LF level): THD ×3.5 from −12 to 0 dBFS at 100 %.
* **A real B–H loop**: at B = 1.5 the loop has area 0.068, coercive field
  ±0.015 (symmetric), remanence 0.165; |M| never exceeds Ms.
* **Memory**: 500 ms after a one-sided excursion (25 flux-leak time
  constants later) a quiet probe comes out differently after a positive and a
  negative history (difference −60.5 dB re probe; CLASSIC −85 dB, i.e. only
  its filter tail).

Integration: one explicit Euler step per oversampled sample (≥ 88.2 kHz,
where the per-sample change of B is a tiny fraction of the loop; Heun and
Euler agree to the 6th digit of the output). The Langevin function uses a
4-term series for |x| < 0.3 and one single-precision exponential elsewhere
(max. relative error 5e-6), while M is integrated in double. Cost:
34 ns per oversampled sample and channel (first version with Heun and
double-precision exp: 112 ns).

When the model is switched on from CLASSIC, the idle core is first brought
to the current flux along its initial magnetisation curve (64 steps), so the
fade starts from a physically consistent state.

## CLASSIC (2.0)

```
φ_h    = φ − hc · sigmoid(dφ/dt)      direction-dependent offset (hysteresis-like memory)
e      = −depth · k · φ_h³ / (1 + φ_h²)       soft cubic → mostly odd harmonics
```

`k = 0.5·a² + 0.15·a`, `hc = 0.04·a`; amount 0 is an exact bypass.

## Measurements (4x, `measurements/nonlinear_thd.csv`)

THD (%) at 50 Hz, 100 % amount (rows: model; columns: level in dBFS):

| model | −40 | −30 | −24 | −18 | −12 | −6 | 0 |
|---|---|---|---|---|---|---|---|
| HYSTERESIS | 0.068 | 0.142 | 0.192 | 0.275 | 0.755 | 2.88 | 2.66 |
| CLASSIC | 0.001 | 0.014 | 0.057 | 0.223 | 0.847 | 2.76 | 5.97 |

THD (%) at −12 dBFS by amount:

| | 25 % | 50 % | 75 % | 100 % |
|---|---|---|---|---|
| HYSTERESIS, 50 Hz | 0.070 | 0.179 | 0.369 | 0.755 |
| CLASSIC, 50 Hz | 0.095 | 0.270 | 0.521 | 0.847 |
| HYSTERESIS, 1 kHz | 0.0006 | 0.0016 | 0.0030 | 0.0047 |
| CLASSIC, 1 kHz | 0.0000 | 0.0002 | 0.0007 | 0.0021 |

Both models: odd harmonics only (H2 −170 dBc, i.e. none, on a symmetric
signal), DC 0.0, programme level change +0.07 dB (HYSTERESIS, 100 %).

Aliasing, HYSTERESIS 100 % at −3 dBFS, worst non-harmonic product (dBc):

| freq | 1x | 2x | 4x | 8x |
|---|---|---|---|---|
| 1 kHz | −121.1 | −136.9 | −147.1 | −156.7 |
| 5 kHz | −111.0 | −133.6 | −140.0 | −142.3 |
| 10 kHz | −103.1 | −123.2 | −133.9 | −154.6 |
| 15 kHz | −115.9 | −126.8 | −126.9 | −137.4 |

Small-signal response (shared shelf and HF softening) at 100 % (−40 dBFS):
20 Hz +1.52, 40 Hz +1.32, 90 Hz +0.78, 200 Hz +0.26, 1 kHz +0.01, 10 kHz
−0.39, 15 kHz −0.77 dB.

| Check | CLASSIC | HYSTERESIS |
|---|---|---|
| LF-dependent saturation | PASS (≈ 400×) | PASS (≈ 160× at −12 dBFS) |
| THD rises with amount | PASS | PASS |
| DC offset | 0.0 | 0.0 |
| Exact bypass at 0 % | PASS | PASS |
| Finite and bounded, +18 dBFS, 10 Hz … 20 kHz, 8x | PASS | PASS (peak 9.3) |
| Click-free model switch | — | PASS |

## Listening

`UNVERIFIED — ENVIRONMENT LIMITATION`. Renders: `wav/12_iron100_hysteresis.wav`
and `wav/13_iron100_classic.wav` (same settings, model only), `wav/06_…`,
`wav/07_…`.
