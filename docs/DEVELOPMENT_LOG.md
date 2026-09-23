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

---

# HEAT 2.1 — the "next version" list

Requested: mid/side and look-ahead, sidechain EQ beyond the HPF, an output
limiter, a proper transformer hysteresis model for IRON, a multiband option,
GPU-backed meter rendering. Same discipline as 2.0: DSP proven by tests
before the UI, then the plug-in layer, then validation.

## Phase 20 — DSP

Design decisions:

* **Front panel untouched.** Everything lives behind the gear; the
  reference render is pixel-identical to 2.0 (max difference 0).
* **Latency on request.** LOOKAHEAD and the LIMITER add latency; all
  latency-dependent paths (look-ahead, SC listen, limiter, bypass) became
  `CrossfadeDelay`s that move taps with a cross-fade. Both controls are
  non-automatable; the host is told from the message thread.
* **M/S as a rotation.** A scaled rotation of the L/R plane morphs between
  L/R and M/S without ever being singular (a linear blend of the two
  matrices is singular at t ≈ 0.59), so mode changes can glide instead of
  jump. The decode uses the morph position delayed by the core latency.
* **Multiband that respects MIX.** Re-summing Linkwitz–Riley bands gives an
  allpass; mixed with dry at 50 % it cancels completely at each crossover.
  HEAT splits only the detector; the audio gets dynamic SVF shelves that are
  exactly the identity when band gains are equal.
* **True-peak limiter** with a sliding-minimum + box look-ahead (sample
  ceiling guaranteed by construction).
* **IRON hysteresis**: inverse Jiles–Atherton (B-driven, as a voltage-driven
  transformer is), irreversible term written with M_irr eliminated so the
  susceptibility can never go negative.

Calibration of the J–A core (scratch sweeps): the first parameter set
(a = 0.35, k = 0.12) was a weakly magnetic core (μ ≈ 1.3) whose THD barely
rose at high level; a high-permeability set (a = k = 0.03, c = 0.4, μi ≈ 5.4)
gives a Rayleigh region at low level, a proper saturation knee and the same
weight as CLASSIC at the reference level (drive 1 + 3a, depth 0.055a).

Problems found and fixed:

* **Click tests failed on fades.** Linear crossfades leave slope kinks
  (−64 dB second-difference spikes). All new fades use smoothstep. The M/S
  "click" and the multiband "click" were partly level changes (a quiet
  presence tone gets louder once the bass stops ducking it); the click
  metric now compares against the steady state on both sides of a change.
* **Limiter true-peak overshoot +0.47 dB** on noise hits with 4x / 16-tap
  detection → 8x / 48-tap detection (+0.12 dB). A later measurement showed
  up to +0.98 dB on programme with loud energy at 21–24 kHz; low-passing the
  same programme at 21 kHz gives +0.024 dB. That is the reconstruction limit
  of short interpolators (BS.1770 meters share it) and is documented as a
  limitation; oversampled limiting goes on the next-version list.
* **Limiter release never reached exactly 1.0** (float stagnation at
  0.99992) → double-precision envelope.
* **Limiter cost 1.4 %** — the 8-phase accumulation was scalar; GCC/Clang
  vector extensions made it 0.43 %.
* **IRON hysteresis cost 112 ns / sample** → Euler step (the per-sample change of B is
  tiny at ≥ 88.2 kHz), fewer divisions, float Langevin with a 4-term series
  near 0: 34 ns, output unchanged to the 6th digit.
* **Remanence test measured the wrong thing** twice (first the DC-blocker
  tail, then the CLASSIC model's residual flux 60 ms after the burst). The
  final test probes 500 ms (25 flux-leak time constants) later at the
  probe's harmonics only: −60.5 dB (hysteresis) vs −85 dB (classic).
* Test-harness details: a stray label slipped into a test again (caught in
  review before compiling); `note()` takes at most four values.

Result: 89 DSP tests / 2730 checks, all passing; ASan/UBSan clean.

## Phase 21 — Plug-in layer

* 15 appended parameters; order checked by test (2.0 host indices
  unchanged). State version 3 with migration: 2.0 sessions, both A/B slots,
  legacy trees and version-2 user presets keep IRON = CLASSIC; controls a
  preset does not mention reset to neutral.
* Latency: `parameterChanged` → `setLatencySamples` on the message thread,
  async update otherwise. A test first "failed" because the engine only
  picks new parameters up on the next block — the reported value was right.
* 8 new factory presets (43 total).

## Phase 22 — UI

* Advanced panel on four pages with a new glass slider; live band / limiter
  reduction strips (a default of −1 dB showed a stray stub under every
  slider — now hidden by default); live latency.
* GPU meter: JUCE draws the OpenGL renderer first and blends the component
  layer over it, so the chassis leaves a hole where the meter glass is and
  a fragment shader draws the glass interior. Verification needed the
  composited frame: reading the front buffer after a swap returned zeros
  (undefined under GLX), reading the back buffer at the start of the next
  frame works with Mesa's copy-swap. Glass vs CPU meter: mean 0.68 / 255;
  whole editor 0.13 / 255.

## Phase 23 — Validation (2.1)

* DSP 89 / 89, plug-in 17 / 17; ASan/UBSan clean on both; TSan see
  `TESTING.md`.
* pluginval 1.0.4 strictness 10 with editor (OpenGL meter on): SUCCESS.
* CPU re-measured with warm-up and best-of-three; the shared VM still
  varies ±20 %, so feature costs are reported as ranges.

## Phase 24 — Glass-encased gain-reduction meter

Request: make the gain-reduction window feel glass-encased, AAA quality.

* Design, iterated on 2x renders of three meter states and at the default
  75 %: a 9-unit machined bezel (polished edge, satin face, turned step,
  chamfer, seal, corner glints), a recessed back plate, round glass tubes
  with specular lines, and a cover glass (softbox reflection, curved crystal
  reflection, grazing-angle reflection, polished edge) whose reflections
  carry a faint cool tint. A first thin diagonal streak read as a scratch
  and was replaced by the softbox.
* All glass is static: cached in the overlay with the scale, drawn over the
  embers and the needle. Per frame only the glass is repainted.
* Found on the way: at 75 / 125 % the meter's cached layers landed a
  quarter pixel off the grid and were resampled (soft, and the reason 75 %
  used to cost more than 100 %). The component origin now sits on a
  multiple of 4 reference units: sharpness ×2.3 at 75 %, frame cost −40 %
  (75 %) and −48 % (125 %) with the glass included.
* GPU path: the static layers are shared, so the shader is unchanged. The
  seam between the OpenGL and component layers was traced by capturing each
  layer alone: clipping every bezel primitive separately stacked partial
  coverage on the window edge and let the face bleed in. The window now
  reaches into the static bezel, is cut from the finished bezel in one copy,
  and the chassis hole is 2 units larger. Corner seam error 68 → 0 / 255.
* `heat_gpucheck` failed once at 125 % and did not reproduce. Cause: with
  no audio running, the editor's frame clock released the meter between the
  checker's ticks. The checker now also reports the state through the
  telemetry; 3 × 8 repeated runs are identical.
* The render benchmark snapshotted each region with its own origin, which
  placed cached layers differently from the editor at 75 / 125 %; regions
  are now painted on the editor's grid (verified pixel-identical to the
  full render). Old and new meter measured with the same tool and flags.
* Fidelity vs the reference: 12.42 → 12.85 overall (meter region
  17.0 → 22.2, the rest unchanged): the requested deviation.
* Validation: plug-in 17 / 1412 and DSP 89 / 2730 pass; pluginval
  strictness 10 SUCCESS (25 / 25, GPU meter on); `heat_gpucheck` 6 / 6 PASS
  on the Release build.
