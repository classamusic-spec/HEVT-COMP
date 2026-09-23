# Output Limiter

`Source/DSP/TruePeakLimiter.*`. OUTPUT page: LIMITER (on / off, not
automatable because it changes latency) and CEILING (−12 … 0 dBTP, default
−1.0). It sits after OUTPUT and before the bypass cross-fade, so it catches
everything HEAT produces, and it is linked (one gain for both channels: the
image never shifts).

## Algorithm

```
detect   p(m) = max over channels of |x(m)| and the inter-sample peaks on both
                sides of m — 8x polyphase windowed-sinc interpolation,
                48 taps per phase (Kaiser β = 6)
require  r(m) = min(1, ceiling / p(m))
release  r'   = follows r down instantly, recovers with a 60 ms exponential
                (double precision so it reaches exactly 1); r' ≤ r always
hold     h    = sliding minimum of r' over W + 1 samples (monotonic deque)
smooth   s    = box average of h over W samples, W = 1.5 ms
audio    y(n) = x(n − (W − 1 + 24)) · s(n)
```

**Why the ceiling is guaranteed for samples.** Every one of the W values
averaged into `s(n)` is a minimum over a window that contains the detection
of the sample being output and of its predecessor, so `s(n)` never exceeds
their `r'`. Hence `|y| ≤ ceiling` for every sample, and each inter-sample
region is covered by the gains on both of its sides. Below the ceiling the
gain is exactly 1 (a running count of held values below unity snaps the box
sum back to exactly W), so an idle limiter is a bit-exact delay.

**Latency.** W − 1 + 24 = 95 samples at 48 kHz (1.98 ms), reported to the
host only while the LIMITER is on. Switching it on or off cross-fades the
audio between the direct and the delayed tap over 20 ms (smoothstep) while
the gain fades in or out; bypass and SC LISTEN move with it.

**Cost.** The seven fractional phases are accumulated as two 4-lane vectors
(GCC / Clang vector extensions, scalar elsewhere): 0.43 % of one core for a
stereo instance at 48 kHz. The limiter is not computed at all while off.

## Measurements (`measurements/limiter.csv`, independent 16x / 128-tap true-peak meter)

| Signal | Ceiling | Output sample peak | True-peak overshoot |
|---|---|---|---|
| Drum loop with white-noise hits, +12 dB drive, band-limited at 20 kHz | −1 / −0.3 dBTP | at ceiling | +0.007 dB (test) |
| Programme, band-limited at 20 kHz, 0 / +6 / +12 dB drive | −1 dBTP | −1.000 dBFS | ≤ +0.041 dB |
| Sine 997 Hz / 11 kHz at +1.6 dBTP | −1 dBTP | −1.00 dBFS | ≤ 0.0 dB |
| Sine fs/4 at 45° (samples at 0.707 of the peak) | −1 dBTP | −4.01 dBFS | −0.0 dB (the limiter saw the inter-sample peak) |
| Drum loop, full band (white noise to 24 kHz) | −1 dBTP | at ceiling | +0.116 dB |
| Programme, full band, loud white-noise snare | −1 / −0.3 dBTP | at ceiling | up to +0.98 dB |

**Limitation.** The last row is real: energy in the top ~3 kHz below Nyquist
(21–24 kHz at 48 kHz) makes inter-sample peaks that a 48-tap interpolator —
and equally the 12-tap BS.1770 true-peak meter — cannot reconstruct. With the
same programme low-passed at 21 kHz the overshoot is 0.024 dB. Mastered
programme rarely carries much energy there; a fully general solution is to
limit in an oversampled domain (listed for the next version).

Other checks (`LimiterTests`): release 1/e recovery 60.8 ms for a 60 ms
constant; largest per-sample gain step 0.12 dB; stereo gain linked (quiet
channel follows the loud one to 0.01 dB); limiter on/off and ceiling
changes click-free (`LookaheadTests`).
