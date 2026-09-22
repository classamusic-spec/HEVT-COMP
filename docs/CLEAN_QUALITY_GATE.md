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
| AUTOMATION | **PASS** | 6000 blocks of random automation of all 18 parameters with random block sizes 1–2048 and occasional +12 dBFS input: all finite, peak ≤ safety ceiling; mode / detector / tube / iron / quality / bypass changes click-free (second-difference peaks at steady-state level) |
| REALTIME | **PASS** | 0 heap allocations in 3000 randomised audio callbacks with all features exercised (`RealtimeTests`); no locks by design; FTZ/DAZ; output identical for host block sizes 16…4096 (deviation 0.0); ASan/UBSan clean; ThreadSanitizer run on concurrent audio / UI / automation threads (see `TESTING.md`) |
| STATE | **PASS** | 20 randomised session round trips; A/B switch / copy / persistence; 35 presets load exactly; user presets and favourites persist; legacy and corrupt state handled; undo works |

## Listening

`UNVERIFIED — ENVIRONMENT LIMITATION`: the build environment has no audio
output. `heat_measure <dir>` renders the programme through every mode and
detector (`wav/01_…` – `wav/08_…`) for a human listening pass.
