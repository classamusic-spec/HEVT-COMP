# HEAT V2 — Development Log

Chronological engineering log. Each entry records what was built, what was
measured, and decisions that matter later.

---

## Phase 0 — Environment inspection

| Item | Result |
|---|---|
| Repository | Empty (no commits) on branch `claude/vigilant-davinci-jwoty5` |
| OS | Linux x86_64 (Ubuntu 24.04 container, kernel 6.18) — cloud CI-style environment |
| Compilers | GCC 13.3.0, Clang 18.1.3 |
| CMake / generator | CMake 3.28.3, Ninja |
| JUCE | Not present → JUCE **8.0.15** (latest tag) cloned to `/home/user/deps/JUCE`; CMake fetches the same tag automatically when `HEAT_JUCE_DIR` is not given |
| Linux JUCE deps | Installed: libxrandr, libxinerama, libxcursor, libxcomposite, fontconfig, alsa, jack, curl, GL headers |
| Validator | pluginval 1.0.4 (Linux) downloaded — available |
| Sanitizers | ASan + UBSan available (GCC/Clang). TSan runtime available in GCC only (Clang's `libclang_rt.tsan` is not installed) |
| Headless GUI | Xvfb available → editor can be rendered and pluginval GUI tests can run |
| Audio test assets | None in repo → all test signals are synthesised (sines, steps, bursts, noise, drum-like transients) |
| Listening | **No audio output device in this environment.** Every "listen" step is recorded as `UNVERIFIED — ENVIRONMENT LIMITATION`; measurements replace listening where possible and WAV renders are produced for a human to audition |
| macOS / AU | Not available here. AU target is configured in CMake for Apple builds but **UNVERIFIED — ENVIRONMENT LIMITATION** |

Visual reference stored as `design/reference/HEAT_LOCKED_REFERENCE.png`
(1536 × 1024, converted losslessly from the supplied WebP).

## Phase 1 — JUCE / CMake / pass-through

* `CMakeLists.txt`: C++20, JUCE 8.0.15, formats VST3 + Standalone (+ AU on
  Apple). `heat_dsp` is a JUCE-free static library so the DSP can be tested and
  sanitised in isolation.
* Stable parameter IDs (`heat.*.v2`) defined once in `Source/Core/Constants.h`.
* Pass-through processor + placeholder editor.
* **Build:** Release VST3 and Standalone link successfully (2m10s cold build).
* **pluginval 1.0.4, strictness 5, headless:** `SUCCESS` on the pass-through.

## Phase 2–6 — Clean compressor core (proven before any colour)

Built as the JUCE-free `heat_dsp` library and tested in isolation.

* **Gain computer** (`GainComputer`): log-domain feed-forward, hard knee
  and quadratic soft knee. Tests: textbook hard knee to 1e-4 dB, soft knee
  continuous in value and slope, monotonic, 1:1 inert.
* **Detectors**: PEAK (rectified), RMS (true mean-square over 12 ms, AES17
  sine reference so a sine reads its peak level), OPTICAL (light cell:
  1.5 ms rise, 18 ms fall, then the optical ballistics profile).
* **Ballistics**: hold → attack → program-dependent memory → rounding pole.
  Decisions and fixes:
  * The first branching attack/release design measured **5.2 ms** for a
    3.16 ms setting on periodic signals (the detector ripple kept
    re-triggering release). Replaced with *hold-then-attack* (the release
    smoother holds the peak, the attack smoother follows it): 3.16 → 3.23 ms.
  * Single-precision coefficients were ~2 % off at 192 kHz. Ballistics now
    run in double precision with `-expm1(-1/(τ·fs))` coefficients:
    sample-rate independent to 0.01 ms.
* **COMPRESS macro**: one knob drives threshold, ratio, knee, attack/release
  scaling, rounding, memory and makeup through five-anchor monotone cubic
  (Fritsch–Carlson) curves per mode, so GR can never step backwards.
  First calibration had WARM too strong at 50 % → thresholds raised; all
  calibration targets (gentle ≈ 1–3 dB at 25 %, clear ≈ 4–7 dB at 50 %,
  strong ≈ 9–14 dB at 75 %, extreme at 100 %, −12 dBFS programme) pass.
* Test fixes along the way: the RMS pulse test sampled a ripple trough
  (now averages power over whole periods); the knee continuity tolerance
  did not scale with ε (now 2ε).
* Result: 21 DSP tests passing at the end of the phase.

## Phase 7–10 — Stereo, sidechain, optical, modes, MIX alignment

* **HeatEngine**: input gain → sidechain (internal / external crossfade,
  TPT-SVF Butterworth HPF) → compressor → oversampled colour path → MIX →
  output → bypass crossfade → NaN / Inf guard and ±31.6 ceiling. Processed
  in 256-sample chunks with FTZ / DAZ.
* **Constant latency**: the dry path, the gain signal and the no-colour
  path are delayed to the ULTRA round-trip latency — 79 samples ≤ 96 kHz,
  69 above 100 kHz — so switching quality, colour or bypass never changes
  the reported latency, and the MIX is a sample-aligned linear blend.
* **Stereo link**: LINKED (max of L/R detector), PARTIAL, DUAL MONO, with
  25 ms smoothed transitions.
* **Modes**: CLEAN / WARM / DRIVE profiles (curves, knee, timing scale,
  rounding, memory, makeup factor, colour drive / bias / interaction).
* Problems fixed:
  * Mode switching clicked because colour bias / interaction changed once
    per chunk → now per-sample arrays from the compressor.
  * Output depended on host block size because the colour path faded in on
    the first block → the colour path starts engaged at reset. Block-size
    deviation now exactly 0.0.
  * Detector distinctness was weakly asserted → trace-distance metric and a
    4 ms attack floor for OPTICAL.
* Tests: bit-exact neutral null at any MIX, measured latency == reported at
  every quality and sample rate, flat 50 % parallel mix, HPF response,
  external key, SC LISTEN, click-free mode / detector / quality / bypass.

## Phase 11–13 — TUBE, oversampling, IRON (analog quality gate)

* **Oversampler**: polyphase Kaiser half-band FIR stages (139 / 31 / 19
  taps, ≥ 100 dB stop band) for 2x / 4x / 8x; the gain is applied in the
  oversampled domain so the gain modulation itself is band-limited.
* **TUBE**: first version was a piecewise curve whose third-derivative kink
  aliased at −52 dBc and ran too hot. Replaced with the analytic sigmoid
  `u / sqrt(1 + u²)` with bias-driven asymmetry, dynamic bias shift and
  normalised small-signal gain: aliasing −103 dBc at 4x (100 %, −3 dBFS,
  10 kHz), small-signal gain exactly 1.
* **WARM** colour measured 2.5 % THD at the reference level — too dirty for
  "warm"; retuned to 0.52 % (DRIVE 4.3 %, CLEAN 0.026 %).
* **IRON**: leaky flux integrator + soft cubic + hysteresis-like offset,
  plus a low shelf. A second-order shelf left a −0.27 dB dip at 200 Hz →
  first-order shelf: +1.52 dB at 20 Hz, +0.26 at 200 Hz, flat at 1 kHz.
* DC blocking is applied only to the distortion component, so the clean
  signal is never high-passed; measured DC ≤ 1.1e-5.
* Measurements (`heat_measure`): THD / harmonics vs level and amount,
  aliasing at 1x…8x, DC, level compensation, frequency response, CPU. See
  `ANALOG_QUALITY_GATE.md`.

## Phase 14 — Presets, A/B, state, advanced settings

* 35 factory presets in 10 categories (first: *Vocal Glue*, which puts
  every control where the reference image shows it), user presets as
  `*.heatpreset` XML, favourites, host program list.
* A/B slots with copy and persistence; versioned session state
  `<HEAT_STATE version="2">`, legacy bare `<HEAT>` trees accepted; UI scale
  and meter hold stored with the session; undo manager.
* Lock-free meter telemetry (atomic min / max accumulation, consumed by the
  UI with `exchange`).
* Plug-in tests: state round trips, presets, automation fuzz, sample-rate ×
  block-size × layout matrix, sidechain bus, multi-instance independence,
  editor lifecycle. Test fix: round trips compared normalised values for
  choice / bool parameters → now compare plain values.

## Phase 15–16 — Locked UI and GR meter

* Fixed 1536 × 1024 design canvas (the reference itself) scaled with one
  affine transform; every coordinate measured on the reference
  (`Source/UI/Layout.h`). Details and method in `UI_SYSTEM.md`.
* Text placed by ink box: `drawTextInInkBox` solves letter tracking so each
  legend covers the same pixels as in the reference. Bug found: tracking
  was applied per glyph instead of cumulatively → fixed.
* Iterations against the headless snapshot (`heat_snapshot` + comparison
  script): satin noise darkened the panel (asymmetric black / white alpha) →
  mean-preserving noise; knob skirts lit from the wrong side; weak cyan arcs
  and glows; meter spill flooded the glass; a capsule artefact at the meter
  base; the ember ramp fitted stop-by-stop to reference samples; the
  "ADVANCED SETTINGS" title overlapped → measured spacing; IRON icon redrawn.
* UTF-8 in narrow string literals (·, —) corrupted text → `fromUTF8` / ASCII.
* The editor no longer changes the global default LookAndFeel (it would
  leak into other plug-ins in the same host process).
* Result: overall MAE 12.4 / 255 vs the reference, flat panel 2.5.
* pluginval strictness 10 including editor tests: SUCCESS.

## Phase 17–19 — Performance, validation, documentation

* **CPU** (48 kHz, 256-sample blocks, one stereo instance): 0.41 % of a core
  for CLEAN without colour up to 5.03 % for DRIVE + TUBE + IRON at ULTRA.
* **UI render cost** (`heat_snapshot … bench`, software renderer): animated
  regions 1.6–6.1 ms per frame depending on scale; full repaint only on
  open / resize.
* **Realtime test** (`Tests/RealtimeTests.cpp`): a global `operator new`
  hook counts allocations inside `processBlock` — 0 in 3000 callbacks with
  every feature exercised. Concurrency test with audio, UI and automation
  threads.
* **Sanitizers**: `HEAT_SANITIZE` (ASan + UBSan) and `HEAT_SANITIZE_THREAD`
  (TSan) CMake options added. The first ASan run of the plug-in tests found
  a heap-buffer-overflow in the *test harness* (`AutomationTests.cpp`:
  256-sample blocks copied into a 48000-sample buffer, which is not a
  multiple of 256) → the test now renders 256 × 188 samples. After the fix:
  DSP 55 tests / 2446 checks and plug-in 15 tests / 971 checks pass with no
  ASan / UBSan reports. TSan (GCC): Realtime concurrency test, the full
  plug-in suite and the DSP suite — 0 reports.
* **pluginval 1.0.4, strictness 10, in-process, with editor (Xvfb)** on the
  final Release VST3: SUCCESS, 25 / 25 test groups.
* Clang 18 in this container has no TSan runtime installed
  (`libclang_rt.tsan` missing); TSan builds use GCC 13 (`libtsan`).
* Documentation: product spec, DSP architecture, macro, gain computer,
  detectors, ballistics, program dependence, tube, iron, oversampling, UI
  system, parameters, testing, quality gates, README.
