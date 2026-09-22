# The COMPRESS Macro

COMPRESS is not a threshold knob. For each MODE it drives three hidden
curves — threshold, ratio and knee — plus the mode's colour drive and the
auto-makeup factor. Each curve is defined by five anchors (0, 25, 50, 75,
100 %) interpolated with a monotone cubic (Fritsch–Carlson), so a monotone
anchor list can never overshoot or reverse (`Modes/ModeProfiles.cpp`).

Design rules, all enforced by `Tests/CompressMacroTests.cpp`:

* COMPRESS 0 % → ratio 1:1 → no gain reduction at any level.
* Threshold never rises and ratio never falls as COMPRESS increases, and the
  knee never narrows → gain reduction is monotonic at every input level
  (checked at 1001 steps for −30 … +6 dBFS).
* 0.1 % of travel never moves the static GR by more than 0.25 dB.
* With the full engine (detector, ballistics, auto makeup) a 5 % step never
  changes the output level by more than 1.5 dB.

## Anchors

| Mode | | 0 % | 25 % | 50 % | 75 % | 100 % |
|---|---|---|---|---|---|---|
| CLEAN | threshold dB | −6 | −16.5 | −21 | −26.5 | −35 |
| | ratio | 1.0 | 1.5 | 2.5 | 4.5 | 8.0 |
| | knee dB | 6 | 6 | 6 | 6 | 6 |
| WARM | threshold dB | −8 | −18.5 | −22.5 | −28.5 | −36 |
| | ratio | 1.0 | 1.4 | 2.0 | 3.0 | 4.5 |
| | knee dB | 12 | 12 | 13 | 14 | 15 |
| DRIVE | threshold dB | −4 | −15.5 | −21 | −28 | −38 |
| | ratio | 1.0 | 2.0 | 4.0 | 8.0 | 20.0 |
| | knee dB | 3 | 3 | 3 | 3 | 3 |

## Measured static gain reduction (from `measurements/compress_macro.csv`)

GR in dB for a sine at the given peak level:

| Mode | COMPRESS | −18 dBFS | −12 dBFS | −6 dBFS | auto makeup |
|---|---|---|---|---|---|
| CLEAN | 25 % | −0.06 | −1.50 | −3.50 | +0.9 |
| CLEAN | 50 % | −1.80 | −5.40 | −9.00 | +3.2 |
| CLEAN | 75 % | −6.61 | −11.28 | −15.94 | +6.8 |
| CLEAN | 100 % | −14.88 | −20.12 | −25.38 | +12.1 |
| WARM | 25 % | −0.50 | −1.86 | −3.57 | +1.2 |
| WARM | 50 % | −2.33 | −5.25 | −8.25 | +3.4 |
| WARM | 75 % | −7.00 | −11.00 | −15.00 | +7.2 |
| WARM | 100 % | −14.00 | −18.67 | −23.33 | +12.1 |
| DRIVE | 25 % | 0.00 | −1.75 | −4.75 | +1.2 |
| DRIVE | 50 % | −2.25 | −6.75 | −11.25 | +4.7 |
| DRIVE | 75 % | −8.75 | −14.00 | −19.25 | +9.8 |
| DRIVE | 100 % | −19.00 | −24.70 | −30.40 | +17.3 |

The calibration targets (gentle ≈ 1–3 dB at 25 %, clear ≈ 4–7 dB at 50 %,
strong ≈ 9–14 dB at 75 %, extreme at 100 %) for a −12 dBFS signal all pass.

## Auto makeup

`makeup = factor × −GR(−12 dBFS)` using the current curve, clamped to
0 … 18 dB, with a per-mode factor (CLEAN 0.60, WARM 0.65, DRIVE 0.70). It
therefore depends on threshold, ratio and knee together and on the mode, not
on threshold alone; it is deliberately conservative (never more than the
reduction at the reference level) so heavy compression reads slightly
quieter rather than louder. Switchable in the advanced panel (default ON).

## Timing per mode (multipliers on the panel ATTACK / RELEASE)

| | attack | release | rounding pole | program memory |
|---|---|---|---|---|
| CLEAN | 1.0× | 1.0× | — | — |
| WARM | 1.3× | 1.15× | 0.4 ms + 0.35 × attack | weight 0.75, depth 0.55, charge 600 ms, slow stage 5 × release |
| DRIVE | 0.5× | 0.6× | — | — |

The OPTICAL detector overrides the memory (see `PROGRAM_DEPENDENCE.md`).
