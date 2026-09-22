# Oversampling

`Source/Quality/Oversampler.*` — cascade of linear-phase polyphase half-band
FIR stages (Kaiser-windowed sinc, 100 dB design attenuation).

| Stage | Taps (4K+3) | Round-trip latency |
|---|---|---|
| 1 (2x) | 139 | 138 samples @ 2x = 69 base samples |
| 2 (4x) | 31 | 30 @ 4x = 7.5 base samples |
| 3 (8x) | 19 | 18 @ 8x = 2.25 base samples |

Polyphase structure: the up-sampler computes even outputs with a 70-tap FIR
and odd outputs as a pure delay; the down-sampler runs a 70-tap FIR on even
inputs plus half of a delayed odd input. Even taps are normalised to sum to
0.5 → exact unity DC gain.

Quality settings: NORMAL = 2x, HIGH = 4x (default), ULTRA = 8x. Above
100 kHz one stage is dropped (two above 150 kHz). Every quality is padded at
the high rate to the ULTRA latency, so the plugin latency is constant: 79
samples ≤ 96 kHz, 69 samples above.

Only the colour path is oversampled; clean dynamics never are (and when no
colour is active the oversampler does not run at all).

## Measurements

Round trip (no nonlinearity), worst passband deviation 20 Hz – 20 kHz at
48 kHz: 2x 0.0001 dB, 4x 0.0001 dB, 8x 0.0005 dB. 50 % parallel blend of the
oversampled path with dry: flat within 0.001 dB up to 18 kHz (no comb
filtering). Reported latency equals measured impulse delay at every quality
and at 44.1 / 48 / 96 / 192 kHz.

### Alias products (worst non-harmonic component 20 Hz – 20 kHz, dBc)

TUBE 100 %, −3 dBFS (hard drive, ≈ 20 % THD at 1 kHz):

| tone | 1x | 2x | 4x | 8x |
|---|---|---|---|---|
| 1 kHz | −97.1 | −104.7 | −122.2 | −140.0 |
| 5 kHz | −41.0 | −94.2 | −122.0 | −142.5 |
| 10 kHz | −16.9 | −51.1 | −103.2 | −122.0 |
| 15 kHz | −15.9 | −39.8 | −71.4 | −127.6 |

TUBE 50 %, −12 dBFS (typical use):

| tone | 1x | 2x | 4x | 8x |
|---|---|---|---|---|
| 10 kHz | −56.0 | −133.3 | −143.8 | −158.0 |
| 15 kHz | −37.4 | −132.3 | −142.7 | −146.7 |

IRON 100 %, −3 dBFS: 10 kHz −105.1 / −116.5 / −140.6 / −159.0; 15 kHz
−105.3 / −113.8 / −126.0 / −142.8 dBc (1x / 2x / 4x / 8x) — IRON's energy is
in the bass, so it aliases very little.

Conclusion: HIGH (4x) keeps aliases below −100 dBc for everything except a
full-scale 15 kHz tone into 100 % TUBE (−71 dBc); ULTRA reaches −122 dBc or
better in every case. The measurements justify exposing all three settings.
