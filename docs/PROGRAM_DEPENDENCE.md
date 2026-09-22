# Program Dependence

WARM, OPTICAL and AUTO RELEASE use the ballistics' memory stage:

```
y1   fast envelope (panel release)
y2   memory: charges towards depth · y1 with τ_charge while compression is
     sustained, releases with τ_slow
out  y1 + weight · min(0, y2 − y1)
```

* A short transient barely charges the memory → recovery follows the fast
  release.
* Sustained compression fills the memory up to `depth` of the reduction →
  after the signal drops, the first part of the recovery is fast (y1) and
  the rest is slow (y2): a two-stage, history-dependent release.

Only `min(0, y2 − y1)` is used, so the memory can hold reduction back but
never add reduction while compressing.

| Profile | weight | depth | charge | slow stage |
|---|---|---|---|---|
| WARM mode | 0.75 | 0.55 | 600 ms | 5 × release |
| OPTICAL detector | 1.0 | 0.60 | 350 ms | 8 × release |
| AUTO RELEASE (any mode) | ≥ 0.9 | ≥ 0.6 | mode | ≥ 5 × release, fast stage ≤ 0.5 × release |

## Measurements

Ballistics alone (release 80 ms, memory weight 1, depth 0.6, charge 400 ms,
slow 1200 ms), recovery to −1 dB from −10 dB:

| After | Recovery |
|---|---|
| 20 ms hit | 184 ms |
| 3 s sustain | 2232 ms |

Full engine, OPTICAL vs PEAK, release 150 ms, 90 % recovery:

| | after 50 ms burst | after 3 s sustain |
|---|---|---|
| OPTICAL | 1003 ms | 2289 ms |
| PEAK | 346 ms | 347 ms |

## Listening

`UNVERIFIED — ENVIRONMENT LIMITATION`: no audio device is available in the
build environment. WAV renders for audition are produced by
`heat_measure <dir>` (`wav/03_clean_optical_50.wav`,
`wav/04_warm_peak_50.wav`, …).
