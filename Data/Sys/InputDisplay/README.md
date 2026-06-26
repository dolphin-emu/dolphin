# Input Display Skins

The Input Display (Movie → Show Input Display) draws a live picture of each
controller using a **skin**: a `skin.json` manifest plus a library of PNG
textures. This document explains how to edit the bundled skins or write your own.

## Folder layout

```
Sys/InputDisplay/
  assets/            shared texture library (PNGs) + ATTRIBUTION.txt
  gamecube/skin.json GameCube skin manifest
  gba/skin.json      Game Boy Advance skin manifest
```

Each skin lives in its own folder and contains a single `skin.json`. Textures are
*not* duplicated per skin — every skin points at the shared `assets/` library via
its `textures` field, so you can draw from the full set of button images (even
ones a given skin doesn't currently use).

### Which skin is used

Skins are selected automatically, per controller port, from your configured
controller type:

- A port set to **GBA (emulated)** uses the `gba` skin.
- Any other GameCube controller uses the `gamecube` skin.

So the folder names `gamecube` and `gba` are meaningful — to restyle the GameCube
display, edit `gamecube/skin.json`; for GBA, edit `gba/skin.json`.

### Editing without touching the install (recommended)

The loader checks your **user** directory first and falls back to the bundled
copy. Mirror the same layout under your Dolphin user folder and your edits
survive updates:

```
<Documents>/Dolphin Emulator/InputDisplay/gba/skin.json
<Documents>/Dolphin Emulator/InputDisplay/assets/...
```

If you override a `skin.json` there, also provide the `assets/` it references
(copy the bundled folder, then edit).

## Coordinate system

All positions are in **design space** and scaled to the window at load time:

- Window size = `design_w * scale` × `design_h * scale`.
- A point `(x, y)` is drawn at `((x + offset_x) * scale, (y + offset_y) * scale)`.
- Button, stick, and d-pad images are drawn as a `tex_size * scale` square.

Use `offset_x` / `offset_y` to shift all content inside the canvas without
resizing the window.

## Top-level fields

| Field        | Type   | Default      | Meaning |
|--------------|--------|--------------|---------|
| `name`       | string | —            | Display name (informational). |
| `textures`   | string | `"gamecube"` | Subfolder under `InputDisplay/` holding the PNGs. Bundled skins use `"assets"`. |
| `background` | color  | `0xCC000000` | Window background (ARGB). |
| `tex_size`   | int    | `128`        | Source square size for button/stick/d-pad art. |
| `gba_mode`   | bool   | `false`      | If true, treats the `Y` button bit as a force-disconnect flag (GBA convention) instead of the regular connected flag. |
| `layout`     | object | —            | `scale`, `offset_x`, `offset_y`, `design_w`, `design_h`. |
| `colors`     | object | —            | Named ARGB colors referenced elsewhere by name. The key `white` is used by trigger fills. |
| `buttons`    | array  | —            | Button graphics (see below). |
| `sticks`     | array  | —            | Analog stick graphics. |
| `dpad`       | object | —            | D-pad graphics. |
| `triggers`   | array  | —            | Analog/digital trigger bars. |
| `overlays`   | array  | —            | Conditional layers drawn on top (see below). |

### Colors

Colors are ARGB, written as a hex string (`"0xFF00E196"`) or a number. The alpha
byte matters — `0xFF…` is opaque.

Images are **multiply-tinted** by their color: black pixels stay black and white
pixels become the tint. This is how one white-on-black PNG renders in any color.

## Buttons

```json
{ "key": "A", "x": 332, "y": 48, "filled": "a-filled.png", "pressed": "a-pressed.png", "color": "A" }
```

| Field     | Meaning |
|-----------|---------|
| `key`     | Which input lights it up (see **Input keys**). |
| `x`, `y`  | Top-left in design space. |
| `filled`  | Image shown when released. |
| `pressed` | Image shown when held. |
| `color`   | Name from `colors` used to tint both images. |

## Sticks

```json
{ "keyx": "StickX", "keyy": "StickY", "x": 22, "y": 52,
  "gate": "joystick-gate-filled.png", "gate_color": "white",
  "knob": "joystick-ribs-filled.png", "knob_color": "white", "travel": 20 }
```

`gate` is the fixed background; `knob` moves with the axes. `travel` is how far
(in design units) the knob shifts at full deflection. `keyx`/`keyy` are axis keys.

## D-pad

```json
"dpad": {
  "x": 108, "y": 144, "gate": "d-pad-gate-filled.png", "gate_color": "white",
  "pressed": { "Up": "d-pad-pressed-up.png", "Down": "...", "Left": "...", "Right": "..." }
}
```

`gate` is always drawn; each `pressed` image is overlaid when its direction is held.

## Triggers

Triggers render a bar that fills with the analog value and (optionally) a digital
click indicator.

```json
{ "key": "L", "axis": "TriggerLeft", "base": "analog-filled.png",
  "bx": 30, "by": 14, "bw": 116, "bh": 24,
  "fill_y": 20, "fill_h": 12,
  "analog_x": 38, "analog_w": 88,
  "digital_x": 126, "digital_w": 12, "divider_x": 126, "dir": "left" }
```

| Field        | Meaning |
|--------------|---------|
| `key`        | Digital button key; when held, fills to 100%. |
| `axis`       | Analog axis key; set `""` for digital-only (e.g. GBA shoulders). |
| `base`       | Background image; drawn at `bx,by` sized `bw×bh`. |
| `fill_y`,`fill_h` | Vertical position/height of the fill band. |
| `analog_x`,`analog_w` | Horizontal extent the analog fill grows across. |
| `dir`        | `"left"` grows rightward from `analog_x`; `"right"` grows leftward from the right edge. |
| `digital_x`,`digital_w` | A separate block shown when the digital click is held. Set `digital_w` to `0` to disable the digital indicator and divider entirely. |
| `divider_x`  | X of the white divider line between analog and digital zones (ignored when `digital_w` is 0). |

The fill and divider are drawn in the `white` color.

## Overlays (conditional layers)

An overlay is a layer drawn over everything when a named state is active. Use it
for disconnect indicators, recording washes, watermarks, etc.

```json
"overlays": [
  { "when": "disconnected", "fill": "0x73000000",
    "image": "disconnected.png", "tint": "0xFFFF0000",
    "align": "center", "scale": 1.0 }
]
```

| Field     | Default    | Meaning |
|-----------|------------|---------|
| `when`    | —          | State that activates the layer: `"disconnected"`, `"connected"`, or any button key (`"Start"`, `"A"`, `"L"`…). |
| `fill`    | `0` (none) | Full-window ARGB scrim. The alpha controls how much it dims the buttons (`0x73000000` ≈ 45% black). |
| `opacity` | `1.0`      | Multiplier applied to both the scrim and the image. |
| `image`   | none       | Optional texture drawn on top of the scrim. |
| `tint`    | `0` (none) | Multiply-tint for the image; `0` draws it unmodified. |
| `align`   | `"center"` | `"center"` honors `scale`; `"fill"` stretches the image to the whole window. |
| `scale`   | `1.0`      | Image size multiplier when centered. |

Overlays are evaluated in order; multiple can be active at once.

## Input keys

**Buttons** (for `key`, d-pad `pressed`, and overlay `when`):
`A`, `B`, `X`, `Y`, `Z`, `L`, `R`, `Start`, `Up`, `Down`, `Left`, `Right`.

**Axes** (for stick `keyx`/`keyy` and trigger `axis`):
`StickX`, `StickY`, `CStickX`, `CStickY`, `TriggerLeft`, `TriggerRight`.
An empty axis (`""`) reads as no deflection — useful for digital-only inputs.

## Tips

- Start by copying a bundled skin folder and tweaking coordinates.
- A GBA has no analog sticks, C-stick, or analog shoulders — model `L`/`R` as
  digital-only trigger bars (`"axis": ""`, `"digital_w": 0`) as the `gba` skin does.
- Drop new PNGs into `assets/` to extend the shared library; keep
  `ATTRIBUTION.txt` accurate if you add third-party art.
