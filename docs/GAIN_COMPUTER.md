# Gain Computer

`Source/DSP/GainComputer.h` — feed-forward, log domain, reduction only.

Hard knee, input above threshold `T` with ratio `R`:

```
outputDb        = T + (inputDb − T) / R
gainReductionDb = outputDb − inputDb = (1/R − 1)(inputDb − T)
```

Soft knee of width `W` centred on `T` (quadratic, continuous in value and
first derivative), with `over = inputDb − T`:

```
over ≤ −W/2       : 0
|over| < W/2      : (1/R − 1)(over + W/2)² / (2W)
over ≥  W/2       : (1/R − 1) · over
```

Properties verified in `Tests/GainComputerTests.cpp`:

| Check | Result |
|---|---|
| Below threshold → exactly 0 dB | PASS |
| Hard knee equals the textbook formula (R = 1.5 … 20, T = −40 … −6) to 1e-4 dB | PASS |
| 1:1 never reduces gain | PASS |
| Soft knee continuous in value and slope at both knee edges (W = 3 … 18 dB) | PASS |
| At the threshold, GR = (1/R − 1)·W/8 exactly | PASS |
| Static curve monotonic (output never decreases with input) | PASS |
| Soft knee never gives less reduction than the hard knee | PASS |

The last property matters for the COMPRESS macro: widening the knee can only
add reduction, so a non-decreasing knee curve keeps the macro monotonic.

The engine evaluates the curve per sample with smoothed threshold, slope
`(1/R − 1)` and knee, so mode and COMPRESS changes glide instead of stepping.
