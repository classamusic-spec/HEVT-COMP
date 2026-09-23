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
* The gain-reduction meter is built as a glass instrument (requested after
  2.1, see *Meter*): a 9-unit machined bezel instead of the 6-unit rim,
  glass tubes and a cover glass. Same position, size, scale and embers; the
  meter region moves away from the reference (MAE 17.0 → 22.2 / 255,
  overall 12.42 → 12.85), every pixel outside it is unchanged (12.00).

## Components

| Component | File | Notes |
|---|---|---|
| Chassis & legends | `Graphics/SilverSurface.*` | Rim, satin face (vertical gradient + radial glows + mean-preserving noise), lip, grooves, logotype, icons, all static legends; cached once per physical scale |
| Knobs | `UI/HeatKnob.*` | Large (INPUT/OUTPUT), medium (ATTACK/RELEASE, grey track ring), COMPRESS (tick scale, amber heat ring), small (TUBE/IRON/MIX/HPF, scale dots). Static layer (shadow, skirt, spun face) cached at physical resolution; pointer and arcs drawn live |
| Spun face | `surface::makeSpunFace` | Conic reflection `base + A cos2θ + tilt cosθ` + fine concentric grain, per-pixel |
| GR meter | `UI/GainReductionDisplay.*` | Glass instrument. Background (shadow, bezel, back plate, empty tubes) and overlay (scale, tube glass, cover glass) cached per physical scale; ember columns, neon walls, white-hot base, rim reflection and peak needle drawn per frame between the two; only the glass is repainted |
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

### Glass instrument

The meter is drawn as a physical object behind a cover glass, lit from the
top left like the knobs (`design/fidelity/GR_METER_GLASS.png`: before |
after at −3.7 dB | idle | −18 dB). Bottom to top:

| Layer | Content |
|---|---|
| Shadow | a tight contact shadow under a wide ambient one: the instrument sits on the panel |
| Bezel (9 units) | polished outer edge, satin face (diagonal gradient), a turned step, a chamfer in shade at the top and lit at the bottom, a black seal against the glass, glints on the rounded corners |
| Back plate | set deep in the recess: the bezel walls shade it (most at the top), a faint lift in the middle |
| Tubes | round: their walls fall off into shadow |
| Embers, needle | per frame (unchanged) |
| Scale | centre column, ticks, legends, title |
| Tube glass | soft sheen and a specular line on the lit side of each tube, a faint far-wall reflection; visible over the embers and over an empty tube |
| Cover glass | a softbox reflected in the top-left corner, a curved crystal reflection with a crisp lower edge over the top of the pane, more reflection at grazing angles under the bezel, the polished edge of the pane. Reflections carry the faint cool cast of a coated pane, which sets them apart from the warm light behind |

Everything above the embers is static and cached with the scale, so the
glass costs nothing per frame; the needle sits under the glass. Per frame
only the glass is repainted (not the bezel and shadow around it).

**Pixel alignment.** The meter component's origin sits on a multiple of 4
reference units — a whole pixel at 50, 75, 100 and 125 % — so its cached
layers are copied 1:1 instead of being resampled a quarter pixel off the
grid (as they were at 75 / 125 % before). Sharpness of the meter at 75 %
(Laplacian variance) 922 → 2138; frame cost below.

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

The front panel was untouched: the 2.1 render of the reference state was
pixel-identical to the 2.0 render (maximum difference 0) until the glass
meter above. Everything new
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
the chassis and the meter component leave the meter's GPU window
transparent — the glass plus 1.5 units of the static bezel — so what shows
there comes from the GPU:

* **back plate and bezel** — a texture rendered once per scale by the same
  code as the CPU meter (`GainReductionDisplay::paintBackground`), origin
  snapped to the pixel grid so texels land exactly on pixels;
* **warmth, bloom, ember columns, neon walls, white-hot base, rim glow, peak
  needle** — one fragment shader. Shapes are signed-distance fields with
  pixel-width antialiasing; the ember ramp is a 256 × 1 lookup texture built
  from the same gradient stops; colours are interpolated unpremultiplied (as
  `juce::ColourGradient` does) and composited with premultiplied "over" in
  the same order as the CPU painter.

The scale overlay (centre column, ticks, legends, title) and the cover
glass stay in the component layer on top.

**The seam.** Where the two layers meet, both must show the same pixels.
The window therefore reaches into the static bezel, where the GPU texture
and the component layer draw identical pixels; the meter cuts the window out
of its finished bezel layer in one copy (clipping each bezel primitive
separately stacked their partial coverage on the antialiased edge, and the
lighter face under the chamfer bled into the seam); and the chassis hole is
2 units larger than the window, so only one antialiased edge meets the GPU
layer. Seam error at the glass corners: up to 68 / 255 in 2.1, none left
(the largest difference in the meter region is now the shader's own). Per frame the message thread only stores three
numbers (left, right, peak) and triggers a GL repaint: no path
rasterisation, no pixel pushing on the UI thread. If no OpenGL context or
shader is available the editor stays on the CPU meter (the meter only
switches to GPU mode after the shader compiled); GPU METER in the OUTPUT page
switches between the two at any time and is saved with the session.

**Verification (`heat_gpucheck`).** The tool opens the real editor in a
window, reads back (a) the glass exactly as the shader drew it and (b) a
complete presented frame including JUCE's GL-composited UI, and compares
them with the CPU renderings of the same state. It reports the state both
directly and through the processor's telemetry, which the editor's frame
clock reads: with no audio running the clock would otherwise release the
meter towards 0 dB between two ticks, and a capture could catch it
part-way (the cause of one intermittent failure; three repeats of every
case are now identical). Under Xvfb with Mesa (software OpenGL), glass
meter build:

| Scale / state | Glass: mean / p99 / max diff (/255) | Meter in frame: mean / p99 / max | Whole editor: mean / p99 |
|---|---|---|---|
| 100 %, −3.66 dB, peak −6 | 0.68 / 4 / 23 | 0.71 / 3 / 23 | 0.14 / 2 |
| 75 %, −3.66 dB | 0.67 / 4 / 17 | 0.73 / 4 / 17 | 0.16 / 2 |
| 125 %, −3.66 dB | 0.66 / 4 / 27 | 0.70 / 3 / 27 | 0.15 / 2 |
| 100 %, idle (0 dB) | 0.00 / 0 / 0 | 0.32 / 1 / 1 | 0.12 / 2 |
| 100 %, −1.2 dB (half lit) | 1.39 / 6 / 21 | 1.25 / 6 / 21 | 0.18 / 4 |
| 100 %, −18 dB, peak −24 | 0.24 / 3 / 22 | 0.49 / 3 / 22 | 0.13 / 2 |

(2.1 before the glass meter, meter in frame: p99 3 / 18 / 11 and max
68 / 66 / 63 at 100 / 75 / 125 %.)

`design/fidelity/GPU_vs_CPU_METER.png` shows GPU | CPU | difference × 8.
Hardware GPU timing and HiDPI (render scale > 1) could not be measured here
(no GPU, no HiDPI display): `UNVERIFIED — ENVIRONMENT LIMITATION`.

## Sizing

Aspect ratio locked at 3:2. Default 75 % (1152 × 768), range 50 %
(768 × 512) to 125 % (1920 × 1280); corner resizer or UI SIZE in the
advanced panel; the size is saved with the session. All cached layers are
regenerated at the physical pixel scale (display scale × UI scale), so HiDPI
is sharp; verified by rendering at 75 %, 100 % and 125 %.
