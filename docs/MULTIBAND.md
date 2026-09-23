# Multiband

MULTIBAND page: MULTIBAND (OFF / 2 BAND / 3 BAND), LOW X-OVER (40 Hz …
1 kHz, default 150 Hz), HIGH X-OVER (1 … 12 kHz, default 2.5 kHz; 3 bands),
LOW / MID / HIGH amount (0 … 200 %, default 100 %) with a live reduction
strip per band.

## Design: split the detector, not the audio

A classic multiband compressor splits the audio with Linkwitz–Riley filters
and re-sums the bands. The sum is an allpass, not the input: with HEAT's MIX
control the dry signal would comb against the phase-shifted wet (at 50 % MIX
the LR4 sum cancels completely at each crossover). HEAT therefore splits only
what the compressor listens to:

```
detector:  sidechain ─► LR4 split (24 dB/oct) ─► LOW / MID / HIGH band compressors
audio:     x · g_ref ─► low shelf  @ f1 carrying (g_LOW  − g_ref)
                     ─► high shelf @ f2 carrying (g_HIGH − g_ref)       (3 bands)
           g_ref = MID band gain (3 bands) or HIGH band gain (2 bands)
```

The shelves are Simper SVF shelves with Q = 0.7071: monotonic (no bump at
the corner), midpoint gain exactly half the shelf gain in dB. When all band
gains are equal both shelves are exactly the identity, so neutral multiband
processing is a bit-exact delayed copy and parallel MIX stays phase-true
(measured null residual 0.0 at MIX 100 % and 50 %, 2 and 3 bands). The audio
is never split, so there is nothing to re-sum.

Where the gain is applied follows the rest of HEAT: in the oversampled colour
path the band shelves run at the high rate (coefficients computed once per
base-rate sample and interpolated across the sub-samples, like the gain
itself); in the no-colour path they run at the base rate.

## Band compressors

Each band is a full `CompressorEngine` (same MODE, DETECTOR, link and
makeup logic as the main compressor) with

* `COMPRESS_band = COMPRESS × amount_band` (clamped to 100 %),
* timing relative to ATTACK / RELEASE: LOW × 1.5, MID × 1.0, HIGH × 0.6 —
  the bass breathes slower, the top recovers faster, so neither pumps
  audibly.

The main (full-band) compressor keeps running; switching MULTIBAND on primes
the band compressors for 25 ms and then fades from the full-band gains to
the band gains over 30 ms (smoothstep, in dB). Switching off, or between 2
and 3 bands, fades back, waits until the delayed shelf ratios have drained,
and restarts (click ratio 1.01 against the steady state). The GR meter shows
the deepest band per channel; the COMPRESS ring and colour drive follow it.

## Measurements (`measurements/multiband.csv`, `MultibandTests`)

Bass-pulsed programme (60 Hz at 0.8 with a 2 Hz envelope, plus a quiet
3 kHz tone), COMPRESS 75 %, crossovers 200 Hz / 2 kHz:

| MULTIBAND | 3 kHz envelope swing | LOW band GR | HIGH band GR | Neutral null |
|---|---|---|---|---|
| OFF | 16.4 dB (the bass pumps the presence range) | — | — | 0.0 |
| 2 BAND | 0.56 dB | −12.9 dB | −0.9 dB | 0.0 |
| 3 BAND | 0.13 dB | −12.9 dB | −0.2 dB | 0.0 |

Band amounts: LOW band GR at 0 / 100 / 160 % = 0.00 / −11.6 / −22.1 dB.
Detector bands match the LR4 model within 0.1 dB (−6.02 dB at each
crossover, −24.6 dB one octave into the neighbour band). Shelves: DC /
Nyquist plateaus within 0.35 / 0.6 dB of the set gain, corner at half the
gain within 0.3 dB, monotonic, exact identity at 0 dB.

CPU: + 0.8 … 1.2 % (2 bands) and + 1.3 … 2.4 % (3 bands) of one core over
the WARM baseline at HIGH quality (range over three runs on the shared test
VM).
