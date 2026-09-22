# Testing

Everything below ran in this environment (Linux x86-64 container, GCC 13.3,
Clang 18.1, JUCE 8.0.15). Numbers are copied from test output, not estimated.

## Suites

| Binary | Needs JUCE | Tests | What it proves |
|---|---|---|---|
| `heat_dsp_tests` | no | 55 | The whole signal path: gain computer, COMPRESS macro, detectors, ballistics, stereo link, sidechain HPF, MIX alignment, engine behaviour, TUBE / IRON, aliasing |
| `heat_plugin_tests` | yes (headless GUI) | 15 | The plug-in as a host sees it: state, presets, A/B, undo, automation fuzz, sample rates × block sizes × layouts, external sidechain bus, multiple instances, editor stress, realtime safety and concurrency |
| `heat_measure` | no | — | Measurement tool: CSVs in `docs/measurements`, WAV renders, CPU profile |
| `heat_snapshot` | yes | — | Headless editor renderer used for the visual fidelity check and the render benchmark |

Run one suite or test by substring: `heat_dsp_tests Ballistics`,
`heat_plugin_tests Realtime`.

### DSP (`heat_dsp_tests`, 55 tests, 2446 checks)

| Suite | Tests | Key assertions |
|---|---|---|
| GainComputer | 6 | Unity below threshold; hard knee equals the textbook formula to 1e-4 dB; soft knee continuous in value and slope; soft ≥ hard reduction; static curve monotonic; 1:1 never reduces |
| CompressMacro | 5 | 0 % inert; GR monotonic over 1001 steps × 6 levels × 3 modes (max step 0.25 dB per 0.1 %); gentle / clear / strong / extreme calibration; auto makeup conservative; full-engine sweep |
| Detectors | 5 | PEAK exact rectified level; RMS is true power (AES17 sine reference, pulse train reads true RMS); RMS reads transients lower than PEAK; OPTICAL rises faster than it falls; detectors measurably distinct in the engine |
| Ballistics | 5 | Attack / release follow the requested time constants (within 5 % / 1.5 samples, 44.1–192 kHz); front-panel values measured through the engine; program-dependent recovery; rounding pole; sample-rate independence |
| Stereo | 5 | LINKED identical gain (1e-6 dB); image does not wander; PARTIAL between LINKED and DUAL MONO; DUAL MONO independent; mono layout |
| Sidechain | 6 | HPF −3.01 dB at cutoff, 12 dB/oct; never filters the audible path; reduces bass-driven GR; stable under fast modulation; EXTERNAL key; SC LISTEN |
| MixAlignment | 3 | Neutral processing is a bit-exact delayed copy at any MIX; reported = measured latency for every quality and rate; oversampled wet + dry does not comb-filter |
| Engine | 9 | Modes distinct; OPTICAL two-stage release; mode / detector / TUBE / IRON / QUALITY / bypass changes click-free; every mode × detector × quality combination finite and bounded; GR consistent across sample rates; output independent of host block size |
| Nonlinear | 8 | TUBE THD vs level and amount; H2 → H3 balance with level; small-signal response; IRON LF-dependent saturation and weight; level compensation; exact bypass at 0; no DC; extreme inputs finite and bounded |
| Aliasing | 3 | Oversampler passband flat and images rejected; TUBE and IRON alias products vs oversampling factor |

Click detection: a change is click-free when the peak second difference of
the output around the change stays at the steady-state level of the signal
itself.

### Plug-in (`heat_plugin_tests`, 15 tests, 971 checks)

| Suite | Test | What it does |
|---|---|---|
| State | parameter IDs are stable | The released ID list is frozen |
| State | first instantiation shows Vocal Glue | Default preset loads with every control at the reference position |
| State | session round trip | 20 randomised sessions survive `get/setStateInformation` exactly (plain values) |
| State | A/B switching, copying, persistence | Slots independent, copy both ways, stored in the session |
| State | factory presets | 35 presets, unique names, valid categories, each loads exactly |
| State | user presets and favourites | Save / reload / delete / favourite in a temporary directory |
| State | legacy and corrupt state | Bare `<HEAT>` tree accepted; garbage bytes and empty data ignored without crashing or changing parameters |
| State | undo | Parameter edits undo and redo |
| Automation | fuzz | 6000 blocks of random automation of all 18 parameters, random block sizes 1–2048, occasional +12 dBFS input: all finite, peak ≤ safety ceiling |
| Automation | sample rates × block sizes × layouts | 44.1 / 48 / 88.2 / 96 / 176.4 / 192 kHz × 16…2048-sample blocks plus an odd size (77); stereo and mono layouts; impulse output finite; reported latency equals the engine latency |
| Automation | external sidechain bus | Key on the sidechain bus drives compression of the main signal |
| Automation | multiple instances | Two instances with different settings do not affect each other |
| Automation | editor stress | Open / close / resize the editor repeatedly while audio runs |
| Realtime | no heap allocation | A global `operator new` hook counts allocations made inside `processBlock`: **0** in 3000 callbacks with random block sizes (1–2047), the external sidechain bus enabled, one parameter changed before every callback (cycling through all 18, including MODE, QUALITY and bypass) and a factory preset loaded every 500 callbacks |
| Realtime | concurrent threads | An audio thread (4000 blocks), a UI thread polling the meter telemetry every 2 ms and an automation thread writing random parameters every 0.3 ms run simultaneously; output must stay finite. This is the ThreadSanitizer workload |

## Sanitizers

| Build | Command | Result |
|---|---|---|
| ASan + UBSan (GCC, `-DHEAT_SANITIZE=ON`, RelWithDebInfo) | `heat_dsp_tests` | 55 tests, 2446 checks, 0 failed, **no sanitizer reports** |
| ASan + UBSan | `xvfb-run -a heat_plugin_tests` | 15 tests, 971 checks, 0 failed, **no sanitizer reports** |
| TSan (GCC, `-DHEAT_SANITIZE_THREAD=ON`, RelWithDebInfo) | `xvfb-run -a heat_plugin_tests Realtime` | 2 tests, 0 failed, **0 ThreadSanitizer reports** |
| TSan | `xvfb-run -a heat_plugin_tests` (full suite) | 15 tests, 971 checks, 0 failed, **0 ThreadSanitizer reports** |
| TSan | `heat_dsp_tests` | 55 tests, 2446 checks, 0 failed, **0 ThreadSanitizer reports** |

The first ASan run found one real bug — in the *test harness*, not the
product: the multi-instance test copied 256-sample blocks into a
48000-sample buffer (48000 is not a multiple of 256), overflowing the last
block (`AutomationTests.cpp`, heap-buffer-overflow). The test now renders
`256 × 188` samples. No product code needed changing.

ThreadSanitizer: the concurrency test drives `processBlock`, the meter
telemetry and parameter writes from three threads at once; the full plug-in
suite adds the editor stress test and multi-instance rendering. No data races
were reported. Clang 18 in this container ships no TSan runtime, so the TSan
build uses GCC 13 (`-DCMAKE_CXX_COMPILER=g++ -DHEAT_SANITIZE_THREAD=ON`).

## Plug-in validation

`pluginval 1.0.4 --strictness-level 10 --validate-in-process` on the Release
VST3, under Xvfb so the editor tests run: **SUCCESS** — all 25 test groups passed (plugin info, open cold / warm,
programs, editor, open editor whilst processing, audio processing,
non-releasing processing, state, automation, editor automation, parameter
thread safety, fuzz parameters, bus tests, background-thread state, …).

## Realtime safety by design

* No allocation, locks, file or console I/O on the audio thread; all buffers
  sized in `prepareToPlay` (verified by the allocation hook above).
* Parameters read through cached `std::atomic<float>*`; engine parameters
  smoothed per sample.
* Meter telemetry is lock-free (atomic min / max accumulation, consumed with
  `exchange` by the UI).
* FTZ / DAZ enabled for the duration of each block; NaN / Inf guard and a
  ±31.6 (+30 dBFS) safety ceiling on the output.

## UI tests

* `heat_snapshot` renders the real editor headlessly at the reference state,
  the advanced panel, and at 75 / 100 / 125 % scale; images in
  `design/fidelity/`.
* Fidelity vs the locked reference: overall MAE 12.4 / 255, flat panel 2.5
  (method in `UI_SYSTEM.md`).
* Render benchmark (`heat_snapshot out.png <scale> <gr> bench`): per-frame
  repaint of the animated regions (meter + COMPRESS ring) 1.57 ms at 100 %,
  2.39 ms at 75 %, 6.12 ms at 125 % (software renderer, one core; repeat of
  an earlier run within 0.1 ms); a full editor repaint 11.1 / 11.1 / 22.1 ms,
  which only happens on open / resize. The 75 % frame costing more than
  100 % was not investigated further; fractional pixel alignment of the
  cached layers at that scale is the suspected cause.
* Editor open / close / resize stress under the plug-in tests; pluginval
  editor tests at strictness 10.

## Measurements

`heat_measure docs/measurements` regenerates:

| File | Contents |
|---|---|
| `compress_macro.csv` | Threshold, ratio, knee, GR at −24 … 0 dBFS vs COMPRESS for each mode |
| `attack_release.csv` | Set vs measured attack / release across the panel range |
| `nonlinear_thd.csv` | TUBE / IRON THD and H2…H5 vs frequency, level and amount |
| `aliasing.csv` | Alias products vs oversampling factor |
| `cpu.csv` | CPU per configuration and quality |
| `wav/*.wav` (gitignored) | Programme renders through every mode, detector and colour setting for listening |

## Not verifiable here

* **Listening** — `UNVERIFIED — ENVIRONMENT LIMITATION` (no audio device).
  WAV renders are provided for a human pass.
* **AU / macOS / Windows hosts** — `UNVERIFIED — ENVIRONMENT LIMITATION`.
  The CMake project configures AU on Apple; only Linux VST3 and Standalone
  were built and validated.
* **GPU-accelerated rendering / real display refresh** — the benchmark uses
  JUCE's software renderer under Xvfb.
