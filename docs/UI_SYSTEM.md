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
| Advanced panel | `UI/AdvancedPanel.*` | Dark glass popover; scrim closes it on outside click / Esc |
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

## Sizing

Aspect ratio locked at 3:2. Default 75 % (1152 × 768), range 50 %
(768 × 512) to 125 % (1920 × 1280); corner resizer or UI SIZE in the
advanced panel; the size is saved with the session. All cached layers are
regenerated at the physical pixel scale (display scale × UI scale), so HiDPI
is sharp; verified by rendering at 75 %, 100 % and 125 %.
