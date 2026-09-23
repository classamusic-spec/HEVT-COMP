# Stereo Modes and Look-ahead

## STEREO MODE (GENERAL page): L/R · M/S · MID · SIDE

HEAT compresses (and colours) in a *processing domain* that is a scaled
rotation of L/R:

```
encode   a = c·(c·L + s·R),   b = c·(c·R − s·L)        c = cos θ, s = sin θ
decode   L = a − tan θ · b,   R = b + tan θ · a        θ = t · π/4
```

At t = 0 this is the identity (L/R); at t = 1, `a = (L + R)/2 = M` and
`b = (R − L)/2` (the side signal; its polarity is a convention and only
matters for the asymmetric TUBE, which sees a mirrored side). For every t in
between the matrix is a rotation times a scale, so it is always invertible:
switching modes morphs t over 30 ms (smoothstep) instead of jumping, and the
output stays continuous (measured click ratio 1.00). The encode uses t at
the input and the decode uses the same t delayed by the core latency, so
the round trip is exact during the morph too (round-trip error 1.2e-7).

The sidechain (after the SC EQ) is encoded the same way, so each processing
channel is detected on its own content; STEREO LINK then decides how the two
detectors combine (DUAL MONO = independent mid and side compression).

| Mode | Processing |
|---|---|
| L/R | as HEAT 2.0 |
| M/S | mid and side compressed (and coloured) separately; use DUAL MONO or PARTIAL link |
| MID | only the mid is processed; the side passes untouched (bit-exact, delayed) |
| SIDE | only the side is processed; the mid passes untouched |

In MID / SIDE, the unprocessed channel is replaced by the delayed dry
channel at the output (cross-faded like everything else) and is excluded
from the stereo link so it can never drive the processed one. Mono tracks
ignore STEREO MODE.

Measured (`MidSideTests`): neutral M/S / MID / SIDE null residual 3e-8;
mono material gives identical output in L/R and M/S; loud mid + quiet side
in M/S DUAL MONO: mid −20.0 dB, side −0.00 dB (in L/R both −20.0 dB); MID /
SIDE only: untouched channel error ≤ 6e-8 with DRIVE + TUBE + IRON on the
processed channel.

## LOOKAHEAD (GENERAL page): OFF · 0.5 · 1 · 2 · 5 · 10 ms

The audio path is delayed by the look-ahead while the sidechain is not, so
gain reduction starts before the transient reaches the output. Because the
attack is a one-pole in the dB domain, the overshoot left after a look-ahead
of D with attack τ decays as e^(−D/τ) — and that is what the engine does
(`measurements/lookahead.csv`, 1 kHz step from −40 to −1 dBFS, COMPRESS 80 %):

| Look-ahead | attack 2 ms: measured / predicted | attack 5 ms: measured / predicted | latency (48 kHz) |
|---|---|---|---|
| off | +19.8 dB | +20.9 dB | 79 |
| 0.5 ms | +15.4 / +15.4 | +18.9 / +18.9 | 103 |
| 1 ms | +11.9 / +12.0 | +17.1 / +17.1 | 127 |
| 2 ms | +7.2 / +7.3 | +14.0 / +14.0 | 175 |
| 5 ms | +1.5 / +1.6 | +7.6 / +7.7 | 319 |
| 10 ms | +0.06 / +0.13 | +2.7 / +2.8 | 559 |

LOOKAHEAD changes the plug-in latency, so it is not automatable; the host
is told about the new latency from the message thread. Changing it moves the
audio, SC LISTEN and bypass taps together with a 20 ms smoothstep
cross-fade (click ratio 1.02). Measured latency equals reported latency for
every look-ahead, with and without the limiter, at 44.1 – 192 kHz.
