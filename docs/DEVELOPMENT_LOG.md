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
| Sanitizers | ASan + UBSan available (GCC/Clang). TSan available in Clang |
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
