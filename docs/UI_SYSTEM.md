# UI System

The locked reference (`design/reference/HEAT_LOCKED_REFERENCE.png`,
1536 × 1024) *is* the design canvas. `MainPanel` is a fixed 1536 × 1024
component; the editor scales it uniformly with an affine transform, so every
coordinate in `Source/UI/Layout.h` is a pixel measured on the reference and
proportions can never drift.

## Fidelity method

1. **Measure.** Gridded zoom crops and pixel profiles of the reference gave
   every knob centre and radius, arc range (±122.5°), ring and tick radii,
   label ink boxes, colours, gradients and separator extents.
2. **Draw in code.** No bitmaps from the reference are used. Text is placed
   by *ink box*: `drawTextInInkBox` solves the letter tracking so each legend
   covers exactly the pixels it covers in the reference, with the baseline
   at cap-top + cap-height.
3. **Render headlessly and compare.** `heat_snapshot` renders the real editor
   (Xvfb) at the reference state; a comparison script reports per-region
   mean colour and mean absolute error and writes side-by-side crops.
   Iterations fixed: a darkening bias in the satin noise, skirts lit from the
   wrong side, weak glows, meter spill, and the meter ember ramp, which was
   finally fitted stop-by-stop to reference samples.

Result (`design/fidelity/REFERENCE_vs_RENDER.png`): overall MAE 12.4 / 255;
flat panel 2.5; region means within ≈ 5 levels everywhere. The remaining
error is mostly the reference's AI-render micro-texture.

Deliberate deviations from the reference image:

* Footer typo fixed: "COMPRESION" → "COMPRESSION".
* The ghosted duplicate "GAIN" above "GAIN REDUCTION" (an artefact of the
  generated reference) is not reproduced.

## Components

| Component | File | Notes |
|---|---|---|
| Chassis & legends | `Graphics/SilverSurface.*` | Rim, satin face (vertical gradient + radial glows + mean-preserving noise), lip, grooves, logotype, icons, all static legends; cached once per physical scale |
| Knobs | `UI/HeatKnob.*` | Large (INPUT/OUTPUT), medium (ATTACK/RELEASE, grey track ring), COMPRESS (tick scale, amber heat ring), small (TUBE/IRON/MIX/HPF, scale dots). Static layer (shadow, skirt, spun face) cached at physical resolution; pointer and arcs drawn live |
| Spun face | `surface::makeSpunFace` | Conic reflection `base + A cos2θ + tilt cosθ` + fine concentric grain, per-pixel |
| GR meter | `UI/GainReductionDisplay.*` | Background + overlay layers cached; ember columns, neon walls, white-hot base, rim reflection and peak needle drawn per frame |
| MODE / DETECTOR | `UI/SegmentedSelector.*` | Silver pills; active pill dark ember with hot outline and bloom |
| Header | `UI/Header.*` | Preset pill (prev / name / tags / heart / next), gear, A / B with underline |
| Advanced panel | `UI/AdvancedPanel.*`, `UI/AdvancedSlider.*` | Dark glass popover with four pages (GENERAL / SIDECHAIN / MULTIBAND / OUTPUT); glass sliders with live reduction strips; scrim closes it on outside click / Esc |
| GPU meter | `UI/GpuMeter.*` | OpenGL renderer for the meter glass (see below) |
| Look & feel | `UI/HeatLookAndFeel.*` | Menus, tooltips, dialogs, text entry, resize corner — no stock JUCE styling |
| Icons / logotype | `UI/Icons.*` | HEAT (E with detached top bar, bar-less Λ), gear, heart, chevrons, tube waves, iron core, crosshair, HPF curve |

Typography: Inter Light / Regular / Medium / SemiBold, embedded (SIL OFL).

## Colour semantics

* **Amber** — compression and heat: COMPRESS ring (glows with gain
  reduction: 55 % at rest → 100 % from 3 dB), GR meter, active selections,
  hover on favourite.
* **Cyan** — clean signal accents only: INPUT / ATTACK / RELEASE value arcs
  (from the start of the range, fading towards the pointer), OUTPUT (mirrored,
  from the end — framing the meter as in the reference), preset name glow,
  focus rings.

## Meter

Two tubes (left / right channel). The lit body starts at the current
reduction and burns hotter towards the base; the right tube carries the
peak-hold needle. Scale 0 / −3 / −6 / −12 / −24 dB, piecewise linear through
the reference label positions. Heat (brightness) ramps from dark at rest to
full at 3 dB. Ballistics: instant attack (the deepest value since the last
frame, accumulated lock-free, is always shown), 60 ms weighted release, 1.5 s
peak hold then 15 dB/s fall (METER HOLD toggle). Frames are driven by
`juce::VBlankAttachment` (display refresh, typically 60 Hz), which stops
automatically while the editor is hidden.

## Interaction

* Knobs: vertical/horizontal drag; Shift / Cmd / Ctrl = 8× fine (re-anchored
  so switching precision never jumps); double-click = default; wheel;
  right-click = *Type Value…* / *Reset*; arrows, Page, Home/End, Return =
  type; value bubble while adjusting; tooltips with value + help (COMPRESS
  adds effective threshold, ratio and current GR).
* Selectors: click, arrow keys; selection indicated by fill, text colour and
  outline (not colour alone).
* Header: prev / next, categorised preset menu (favourites, user, save,
  delete), heart, gear, A / B (right-click copies).
* Undo / redo: Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z, Cmd/Ctrl+Y.
* Accessibility: knobs expose a ranged value interface; selectors and meter
  expose text values; all controls have titles; keyboard focus rings.

## Advanced panel (2.1)

The front panel is untouched: a 2.1 render of the reference state is
pixel-identical to the 2.0 render (maximum difference 0). Everything new
lives in the gear popover, now on four pages selected by a pill tab row:

* **GENERAL** — STEREO MODE, STEREO LINK, LOOKAHEAD, AUTO MAKEUP, AUTO
  RELEASE, QUALITY, IRON MODEL
* **SIDECHAIN** — SOURCE, SC LISTEN, HIGH-PASS, LOW-PASS, EQ FREQ / GAIN / Q
* **MULTIBAND** — MULTIBAND, LOW / HIGH X-OVER, LOW / MID / HIGH amount;
  each band slider carries a live reduction strip (0 … −12 dB)
* **OUTPUT** — LIMITER, CEILING (live limiter reduction strip, 0 … −6 dB),
  METER HOLD, GPU METER, UI SIZE

Controls that do nothing in the current configuration are dimmed (the MID
band and HIGH X-OVER outside 3-band mode, the band amounts when multiband is
off, CEILING while the limiter is off). The footer shows the live latency in
samples and milliseconds. The glass slider (`AdvancedSlider`) follows the
knobs' interaction model: drag (Shift / Cmd / Ctrl = fine), double-click
reset, wheel, click the value or Return to type, right-click menu, arrow /
Page / Home / End keys, ranged-value accessibility; bipolar ranges fill from
the centre. The last page shown is remembered for the session.

## GPU meter (2.1)

With GPU METER on (default), the editor attaches a `juce::OpenGLContext`.
JUCE then draws `GpuMeterRenderer::renderOpenGL()` first and blends the
software-painted component layer over it (premultiplied alpha). In GPU mode
the chassis and the meter component leave the meter glass transparent, so
what shows there comes from the GPU:

* **glass background** — a texture rendered once per scale by the same code
  as the CPU meter (`GainReductionDisplay::paintBackground`), origin snapped
  to the pixel grid so texels land exactly on pixels;
* **warmth, bloom, ember columns, neon walls, white-hot base, rim glow, peak
  needle** — one fragment shader. Shapes are signed-distance fields with
  pixel-width antialiasing; the ember ramp is a 256 × 1 lookup texture built
  from the same gradient stops; colours are interpolated unpremultiplied (as
  `juce::ColourGradient` does) and composited with premultiplied "over" in
  the same order as the CPU painter.

The scale overlay (centre column, ticks, legends, title) stays in the
component layer on top. Per frame the message thread only stores three
numbers (left, right, peak) and triggers a GL repaint: no path
rasterisation, no pixel pushing on the UI thread. If no OpenGL context or
shader is available the editor stays on the CPU meter (the meter only
switches to GPU mode after the shader compiled); GPU METER in the OUTPUT page
switches between the two at any time and is saved with the session.

**Verification (`heat_gpucheck`).** The tool opens the real editor in a
window, reads back (a) the glass exactly as the shader drew it and (b) a
complete presented frame including JUCE's GL-composited UI, and compares
them with the CPU renderings of the same state. Under Xvfb with Mesa
(software OpenGL):

| Scale / state | Glass: mean / p99 / max diff (/255) | Whole editor: mean / p99 |
|---|---|---|
| 100 %, −3.66 dB, peak −6 | 0.68 / 4 / 23 | 0.13 / 2 |
| 75 %, −3.66 dB | 0.67 / 4 / 17 | 0.19 / 3 |
| 125 %, −3.66 dB | 0.66 / 4 / 26 | 0.18 / 3 |
| 100 %, idle (0 dB) | 0.00 / 0 / 0 | 0.11 / 2 |
| 100 %, −1.2 dB (half lit) | 1.34 / 6 / 17 | 0.17 / 3 |
| 100 %, −18 dB, peak −24 | 0.24 / 3 / 22 | 0.12 / 2 |

`design/fidelity/GPU_vs_CPU_METER.png` shows GPU | CPU | difference × 8.
Hardware GPU timing and HiDPI (render scale > 1) could not be measured here
(no GPU, no HiDPI display): `UNVERIFIED — ENVIRONMENT LIMITATION`.

## Sizing

Aspect ratio locked at 3:2. Default 75 % (1152 × 768), range 50 %
(768 × 512) to 125 % (1920 × 1280); corner resizer or UI SIZE in the
advanced panel; the size is saved with the session. All cached layers are
regenerated at the physical pixel scale (display scale × UI scale), so HiDPI
is sharp; verified by rendering at 75 %, 100 % and 125 %.
