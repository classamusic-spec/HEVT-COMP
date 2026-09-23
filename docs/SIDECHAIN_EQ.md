# Sidechain EQ

SIDECHAIN page: HIGH-PASS (the front-panel HPF), LOW-PASS (1 … 20 kHz,
20 kHz = off), EQ FREQ (80 Hz … 12 kHz), EQ GAIN (±18 dB, 0 = off),
EQ Q (0.3 … 8). All of it shapes only what the detector hears; the audible
signal is never filtered (measured null residual 0.0 with COMPRESS 0 and the
most extreme EQ).

```
sidechain ─► HPF (2nd-order Butterworth) ─► LPF (2nd-order Butterworth) ─► BELL ─► detectors
```

All three are topology-preserving-transform state-variable filters
(`Source/DSP/SidechainFilter.*`): unconditionally stable under modulation.
Frequencies glide in the log domain over 30 ms, the bell gain in dB and Q in
the log domain; coefficients are refreshed every 16 samples while anything
moves. Every section keeps running while bypassed, so switching one in never
starts from stale state.

* **LOW-PASS** fades out of the circuit between 16 and 20 kHz and is exactly
  bypassed at 20 kHz (bit-identical output to the 2.0 detector path).
* **BELL** is the Simper SVF peaking filter: `A = 10^(dB/40)`,
  `k = 1 / (Q·A)`, `out = x + k (A² − 1) · bandpass`. Gain at the centre is
  exactly the set gain; 0 dB is an exact identity.

SC LISTEN plays the filtered detector signal (L/R, latency aligned), so the
EQ can be tuned by ear.

## Measurements (`SidechainEqTests`, `measurements/sidechain_eq.csv`)

| Check | Result |
|---|---|
| LPF at 1 / 4 / 12 kHz | −3.01 dB at the cutoff; −12.4 / −13.5 / −12.0 dB an octave above; 0.00 dB a decade below |
| LPF at 20 kHz | bit-identical to no LPF |
| BELL +12 / −9 / +18 dB, Q 0.5 / 1 / 4 | centre gain within 0.15 dB of the setting and of the analytic model |
| BELL width | +12 dB at 3 kHz, one octave above: Q 4 → +0.38 dB, Q 0.5 → +7.1 dB |
| De-esser | equal-level 1 kHz and 7 kHz tones, +15 dB bell at 7 kHz (Q 1.5): GR 7 kHz −1.6 → −10.5 dB, 1 kHz −1.6 → −1.7 dB |
| Fast sweeps of every EQ control while compressing | finite, bounded |

The CSV lists the complete detector-path magnitude for four settings
(default; HPF 120 Hz + LPF 6 kHz; +15 dB at 7 kHz; −9 dB at 300 Hz).
