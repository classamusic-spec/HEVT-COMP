# Clean Quality Gate

Clean compression was proven before any analog colour was built (see the
development log for the order). Every line below is backed by an automated
test that ran in this environment; the measured values are copied from the
test output.

| Item | Status | Evidence |
|---|---|---|
| STATIC CURVE | **PASS** | `GainComputerTests`: textbook hard knee to 1e-4 dB; soft knee continuous in value and slope; monotonic; 1:1 inert |
| COMPRESS MACRO | **PASS** | `CompressMacroTests`: 0 % inert; GR monotonic at 1001 steps × 6 levels × 3 modes; max step 0.25 dB per 0.1 %; calibration targets met in all modes; full-engine output steps < 1.5 dB per 5 % |
| ATTACK | **PASS** | Engine step test: 3.16 → 3.23 ms, 17.8 → 17.9 ms, 100 → 100.04 ms; ballistics within 5 % / 1.5 samples at 44.1–192 kHz |
| RELEASE | **PASS** | 20 → 20.0, 245 → 244.9, 857 → 857.0, 3000 → 2999 ms; sample-rate independent to 0.01 ms (double precision) |
| PEAK | **PASS** | Instantaneous rectified level exact; catches transients hardest (−14.7 dB vs RMS −6.1 dB on drum hits) |
| RMS | **PASS** | True power: pulse-train reading −8.93 dB = true RMS (abs-mean would read −18.93 dB); sine ripple 0.029 dB |
| STEREO | **PASS** | Linked L/R GR difference 1e-6 dB; programme image mismatch 5e-6 dB; partial / dual mono behave as specified; mono layout works |
| HPF | **PASS** | −3.01 dB at cutoff, −12.30 dB an octave below, 0.000 dB a decade above; audible path untouched (deviation 0.0); stable under 20↔400 Hz block-rate modulation |
| MIX ALIGNMENT | **PASS** | Neutral null residual 0.0 at MIX 0/30/50/100 %; measured latency = reported at every quality and 44.1/48/96/192 kHz; 50 % parallel with oversampled path flat within 0.001 dB to 18 kHz |
| AUTOMATION | **PASS** | 6000 blocks of random automation of all 33 parameters with random block sizes 1–2048 and occasional +12 dBFS input: all finite, peak ≤ safety ceiling; mode / detector / tube / iron / quality / bypass changes click-free (second-difference peaks at steady-state level) |
| REALTIME | **PASS** | 0 heap allocations in 3000 randomised audio callbacks with all features exercised, 2.1 included (`RealtimeTests`); no locks by design; FTZ/DAZ; output identical for host block sizes 1…4096 with every feature on (deviation 0.0); ASan/UBSan clean; ThreadSanitizer: see `TESTING.md` |
| STATE | **PASS** | 20 randomised session round trips; A/B switch / copy / persistence; 43 presets load exactly; user presets and favourites persist; legacy and corrupt state handled; undo works; 2.0 sessions, A/B slots and user presets migrate (IRON stays CLASSIC, new controls neutral) |

## 2.1 additions

| Item | Status | Evidence |
|---|---|---|
| LOOKAHEAD | **PASS** | Reported = measured latency for every setting × LIMITER at 44.1–192 kHz; onset overshoot follows e^(−D/τ): 2 ms → +7.15 dB (predicted +7.27), 5 ms → +1.49 dB (+1.62); changes click-free (ratio 1.02) |
| STEREO MODES | **PASS** | Rotation round trip 1.2e-7; neutral M/S / MID / SIDE null 3e-8; M/S dual mono: loud mid −20.0 dB, quiet side −0.00 dB; MID / SIDE only: untouched channel bit-exact (≤ 6e-8); mode changes click-free (ratio 1.00) |
| SIDECHAIN EQ | **PASS** | LPF −3.01 dB at cutoff, exact bypass at 20 kHz; bell centre gain within 0.15 dB; +15 dB at 7 kHz: sibilant GR −10.5 dB vs −1.6 dB, vocal tone unchanged; audible path untouched (null 0.0) |
| LIMITER | **PASS** (with a documented limitation) | Sample peak never above the ceiling; true peak ≤ +0.04 dB for 20 kHz band-limited programme, +0.12 dB for full-band noise hits, up to +0.98 dB when there is loud energy at 21–24 kHz (see `LIMITER.md`); exact bypass below the ceiling; 60.8 ms release; linked |
| MULTIBAND | **PASS** | Neutral null 0.0 at MIX 100 / 50 % (no crossover allpass in the audio); presence-range pumping 16.4 → 0.13 dB (3 bands); band amounts 0 / 100 / 160 % → 0.0 / −11.6 / −22.1 dB; transitions click-free (ratio 1.01) |
| ALL FEATURES | **PASS** | Every 2.1 feature at once: output identical for host blocks 1 … 4096; 400 random settings every 600 samples finite and bounded |

## Listening

`UNVERIFIED — ENVIRONMENT LIMITATION`: the build environment has no audio
output. `heat_measure <dir>` renders the programme through every mode and
detector (`wav/01_…` – `wav/08_…`) for a human listening pass.
