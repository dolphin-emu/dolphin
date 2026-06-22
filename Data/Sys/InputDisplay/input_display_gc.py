# Bundled GameCube input-display overlay. Driven natively by InputDisplayController,
# never by the Scripts window. Composites m-overlay's button textures from a JSON skin;
# during DTM playback it reads the played-back frame via get_gc_buttons_display.

import json
import os

from dolphin import gui, event, controller, utils

PORT = 0
SKIN_FILE = "gamecube.json"


def _skin_dirs():
    # Prefer a writable user InputDisplay/ dir; fall back to the bundled Sys copy.
    dirs = []
    try:
        user = os.path.normpath(os.path.join(utils.get_script_dir(), "..", "..", "InputDisplay"))
        dirs.append(user)
    except Exception:
        pass
    dirs.append(os.path.dirname(os.path.abspath(__file__)))
    return dirs


def _load_skin():
    for d in _skin_dirs():
        path = os.path.join(d, SKIN_FILE)
        if os.path.isfile(path):
            try:
                with open(path, "r", encoding="utf-8") as f:
                    return json.load(f), d
            except (OSError, ValueError):
                pass
    return {}, _skin_dirs()[-1]


def _color(value, fallback=0):
    if isinstance(value, int):
        return value
    if value is None:
        return fallback
    return int(str(value), 16)


SKIN, SKIN_DIR = _load_skin()
TEX_DIR = os.path.join(SKIN_DIR, SKIN.get("textures", "gamecube"))
COLORS = SKIN.get("colors", {})
LAYOUT = SKIN.get("layout", {})
SCALE = LAYOUT.get("scale", 0.5)
OFF_X = LAYOUT.get("offset_x", 0)
OFF_Y = LAYOUT.get("offset_y", 0)
TEX = SKIN.get("tex_size", 128)
BG = _color(SKIN.get("background"), 0xCC000000)
W = max(1, round((LAYOUT.get("design_w", 522)) * SCALE))
H = max(1, round((LAYOUT.get("design_h", 304)) * SCALE))

cv = gui.canvas("Input Display", W, H, overlay=True)


def _read_pad():
    try:
        return controller.get_gc_buttons_display(PORT)
    except AttributeError:
        return controller.get_gc_buttons(PORT)


def _tex(name):
    return os.path.join(TEX_DIR, name)


def _sx(v):
    return (v + OFF_X) * SCALE


def _sy(v):
    return (v + OFF_Y) * SCALE


def _img(name, x, y, w, h, tint=0, src=(0.0, 0.0, 1.0, 1.0)):
    cv.image(_tex(name), (_sx(x), _sy(y)), (w * SCALE, h * SCALE), tint=tint, src=src)


def _rect(x0, y0, x1, y1, color):
    cv.rect_filled((_sx(x0), _sy(y0)), (_sx(x1), _sy(y1)), color)


def _line(x0, y0, x1, y1, color, thick=2):
    cv.line((_sx(x0), _sy(y0)), (_sx(x1), _sy(y1)), color, thick)


def _tint(key):
    # Textures are black-body/white-ring; a multiply tint keeps the body black and colors the ring.
    return _color(COLORS.get(key)) if key else 0


# Buttons: unpressed shows the filled body (black + colored ring); pressed lights up the color.
def _draw_button(b, pads):
    name = b.get("pressed") if pads.get(b.get("key")) else b.get("filled")
    if name:
        _img(name, b["x"], b["y"], TEX, TEX, _tint(b.get("color")))


def _draw_stick(s, pads):
    if s.get("gate"):
        _img(s["gate"], s["x"], s["y"], TEX, TEX, _tint(s.get("gate_color")))
    ks = s.get("knob_size", TEX)
    travel = s.get("travel", 20)
    cx, cy = s["x"] + TEX / 2, s["y"] + TEX / 2  # gate center
    nx = (pads.get(s.get("keyx"), 128) - 128) / 128.0
    ny = (pads.get(s.get("keyy"), 128) - 128) / 128.0
    # Screen Y grows downward, so a stick pushed up moves the knob up. Knob is centered on the gate.
    if s.get("knob"):
        _img(s["knob"], cx - ks / 2 + nx * travel, cy - ks / 2 - ny * travel, ks, ks,
             _tint(s.get("knob_color")))


def _draw_dpad(d, pads):
    if d.get("gate"):
        _img(d["gate"], d["x"], d["y"], TEX, TEX, _tint(d.get("gate_color")))
    for key, name in d.get("pressed", {}).items():
        if pads.get(key):
            _img(name, d["x"], d["y"], TEX, TEX)


def _draw_trigger(t, pads):
    # Base bar (black fill + white outline), then white analog fill, digital-press block, divider line.
    _img(t["base"], t["bx"], t["by"], t["bw"], t["bh"])
    fy, fh = t["fill_y"], t["fill_y"] + t["fill_h"]
    ax, aw = t["analog_x"], t["analog_w"]
    digital = bool(pads.get(t.get("key")))
    val = 1.0 if digital else pads.get(t.get("axis"), 0) / 255.0
    white = _color(COLORS.get("white"), 0xFFFFFFFF)
    if t.get("dir") == "right":
        right = ax + aw
        _rect(right - aw * val, fy, right, fh, white)
    else:
        _rect(ax, fy, ax + aw * val, fh, white)
    if digital:
        _rect(t["digital_x"], fy, t["digital_x"] + t["digital_w"], fh, white)
    _line(t["divider_x"], fy, t["divider_x"], fh, white)


def _redraw():
    cv.clear()
    cv.rect_filled((0, 0), (W, H), BG)
    try:
        pads = _read_pad()
        if not pads.get("Connected", True):
            cv.text((W / 2 - 36, H / 2 - 6), 0xFFAAAAAA, "No controller")
        else:
            for s in SKIN.get("sticks", []):
                _draw_stick(s, pads)
            if "dpad" in SKIN:
                _draw_dpad(SKIN["dpad"], pads)
            for b in SKIN.get("buttons", []):
                _draw_button(b, pads)
            for t in SKIN.get("triggers", []):
                _draw_trigger(t, pads)
    except Exception as e:
        cv.text((6, 12), 0xFFFF6060, "err: " + str(e)[:44])
    cv.commit()


_redraw()
event.on_frameadvance(_redraw)
