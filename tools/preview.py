#!/usr/bin/env python3
"""Render Escape Time faces on the host, exactly as the watch computes them.

    python3 tools/preview.py [out_dir]     (default: preview/)

The whole face comes from the watch's own src/c/mandel.c, dive.c, grid.c
and face.c, compiled for the host and called through ctypes; the digits
are the real resources, laid out as src/c/layout.c does; colors and B&W
dither levels come from tools/design.py, as src/c/generated/palette.h does,
and B&W faces are dithered as src/c/paint.c does. So a preview is the watch
face, pixel for pixel, at any time of day (the emulator only shows the
current time).

Every image comes in two versions: exact Pebble colors, and `-screen`, as
the color display shows them (muted; see tools/display.py). Judge the look
by the -screen versions.

Writes:
  hours-<platform>.png   all 12 dives at :00, :10, ... :59 (rows: hours 0-11)
  platforms.png          one moment on all seven watches
  platforms-dark.png     the same in dark mode (a setting)
  fractal.png            the raw fractal per hour and depth, without digits
  cycle-<platform>.gif   one full palette cycle at one moment (the motion)
"""

import ctypes
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import design  # noqa: E402
import display  # noqa: E402

FPS = design.FPS

# name: (width, height, round, color, resource tag, grid cell shift). The
# shift is the one main.c ends up with: aplite's heap only fits 2 px cells.
PLATFORMS = {
    "aplite": (144, 168, False, False, "144w", 1),
    "basalt": (144, 168, False, True, "144w", 0),
    "chalk": (180, 180, True, True, "180w", 0),
    "diorite": (144, 168, False, False, "144w", 0),
    "emery": (200, 228, False, True, "200w", 0),
    "flint": (144, 168, False, False, "144w", 0),
    "gabbro": (260, 260, True, True, "260w", 0),
}

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
INSIDE = 0xFFFF


class View(ctypes.Structure):
    _fields_ = [("cr", ctypes.c_int32), ("ci", ctypes.c_int32),
                ("step", ctypes.c_int32), ("max_iter", ctypes.c_uint16)]


def load_lib():
    lib_path = os.path.join(ROOT, "tools", "host", "librender.so")
    sources = [os.path.join(ROOT, p) for p in
               ("tools/host/render.c", "src/c/mandel.c", "src/c/dive.c",
                "src/c/grid.c", "src/c/face.c")]
    headers = [os.path.join(ROOT, "src/c", p) for p in
               ("generated/targets.h", "generated/palette.h", "mandel.h", "dive.h",
                "grid.h", "face.h")]
    newest = max(os.path.getmtime(p) for p in sources + headers)
    if not os.path.exists(lib_path) or os.path.getmtime(lib_path) < newest:
        subprocess.check_call(["cc", "-O2", "-shared", "-fPIC", "-Wall", "-Wextra",
                               "-o", lib_path] + sources)
    return ctypes.CDLL(lib_path)


LIB = load_lib()
LIB.dist_create.restype = ctypes.c_void_p
LIB.dist_create.argtypes = [ctypes.c_uint16, ctypes.c_uint16]
LIB.dist_destroy.argtypes = [ctypes.c_void_p]
LIB.dist_merge_row.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                               ctypes.c_char_p, ctypes.c_int]
LIB.render_face_grid.argtypes = [ctypes.c_int] * 5 + [ctypes.c_char_p]
LIB.render_face.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]


def glyph_metrics(width, compact=False):
    """A digit set's GLYPH_* constants from src/c/generated/glyphs.h."""
    text = open(os.path.join(ROOT, "src/c/generated/glyphs.h")).read()
    block = re.search(r"PBL_DISPLAY_WIDTH == %d\n(.*?)#(?:el|endif)" % width, text, re.S).group(1)
    name = "S" if compact else "L"
    m = {k: int(v) for k, v in re.findall(r"#define GLYPH_%s_(\w+) (\d+)\n" % name, block)}
    style = re.search(r"#define GLYPH_%s_STYLE \{([\d, ]+)\}" % name, block).group(1)
    m["STYLE"] = bytes(int(v) for v in style.split(","))
    return m


def glyph_layout(m, screen_w, area_top, area_h):
    """Digit cell origins, as src/c/layout.c layout_compute."""
    w, h, gap = m["WIDTH"], m["HEIGHT"], m["GAP"]
    step_x, step_y = (w + gap + 1) & ~1, (h + gap + 1) & ~1
    left = ((screen_w - (step_x + w)) // 2) & ~1
    top = (area_top + (area_h - (step_y + h)) // 2) & ~1
    return [(left + (i % 2) * step_x, top + (i // 2) * step_y) for i in range(4)]


class Dist:
    """The watch's DistField of four digits (src/c/face.c), as main.c builds it."""

    def __init__(self, width, height, digits, tag):
        self.ptr = LIB.dist_create(width, height)
        m = glyph_metrics(width)
        cols, rows, pad = m["COLS"], m["ROWS"], m["PAD"]
        for d, (x, y) in zip(digits, glyph_layout(m, width, 0, height)):
            data = open(os.path.join(ROOT, "resources/glyphs/%d~%s.bin" % (d, tag)), "rb").read()
            for r in range(rows):
                LIB.dist_merge_row(self.ptr, (x - pad) // 2, (y - pad) // 2 + r,
                                   data[r * cols:(r + 1) * cols], cols)
        self.style = m["STYLE"]

    def __del__(self):
        LIB.dist_destroy(self.ptr)


# The codes render_face writes: the palette, then digit, keyline, inside, lace.
# Per look (design.MODES), as main.c's s_colors.
FACE_COLORS = {m: design.palette(m) + list(design.face_colors(m)) for m in design.MODES}
BW_LEVELS = {m: design.bw_levels(m) + list(design.bw_face_levels(m)) for m in design.MODES}


def face_grid(hour, minute, w, h, shift):
    cell = 1 << shift
    buf = ctypes.create_string_buffer(((w + cell - 1) >> shift) * ((h + cell - 1) >> shift))
    if not LIB.render_face_grid(hour, minute, w, h, shift, buf):
        raise MemoryError("render_face_grid")
    return buf


def face_codes(platform, hour, minute, phase, clock24, state):
    """The face's color codes, row by row, and the minute's state."""
    w, h, _, _, tag, shift = PLATFORMS[platform]
    if state is None:
        shown = hour if clock24 else (hour % 12 or 12)
        state = (face_grid(hour, minute, w, h, shift),
                 Dist(w, h, [int(c) for c in "%02d%02d" % (shown, minute)], tag))
    grid, dist = state
    out = ctypes.create_string_buffer(w * h)
    LIB.render_face(grid, w, h, shift, dist.ptr, dist.style, phase, out)
    return out.raw, state


def cells(hour, minute, cw, ch):
    buf = (ctypes.c_uint16 * (cw * ch))()
    view = View()
    LIB.render_time(hour, minute, cw, ch, ctypes.byref(view), buf)
    n = design.PALETTE_SIZE
    return [0xFF if v == INSIDE else v % n for v in buf]


def rgb(c):
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255)


def face(platform, hour, minute, phase=0, clock24=True, state=None, mode="light"):
    """The face at a time (24-hour `hour` drives the dive), and the state to
    pass back to render other phases of the same minute. B&W watches dither
    each code's level, as src/c/paint.c does. `mode`: the look (design.MODES)."""
    w, h, is_round, color, _, _ = PLATFORMS[platform]
    codes, state = face_codes(platform, hour, minute, phase, clock24, state)
    img = Image.new("RGB", (w, h))
    if color:
        colors = [rgb(c) for c in FACE_COLORS[mode]]
        img.putdata([colors[c] for c in codes])
    else:
        img.putdata([(255, 255, 255) if BAYER[i // w & 3][i % w & 3] < BW_LEVELS[mode][c] else (0, 0, 0)
                     for i, c in enumerate(codes)])
    return (round_mask(img) if is_round else img), state


def round_mask(img):
    circle = Image.new("L", img.size, 0)
    ImageDraw.Draw(circle).ellipse((0, 0, img.width - 1, img.height - 1), fill=255)
    return Image.composite(img, Image.new("RGB", img.size), circle)


def sheet(tiles, cols, label_h=14, pad=6, bg=(24, 24, 24)):
    tw, th = tiles[0][0].size
    rows = (len(tiles) + cols - 1) // cols
    img = Image.new("RGB", (cols * (tw + pad) + pad, rows * (th + label_h + pad) + pad), bg)
    d = ImageDraw.Draw(img)
    for i, (tile, label) in enumerate(tiles):
        x = pad + (i % cols) * (tw + pad)
        y = pad + (i // cols) * (th + label_h + pad)
        img.paste(tile, (x, y))
        d.text((x, y + th + 2), label, fill=(200, 200, 200))
    return img


def fractal(hour, minute, w=135, h=146):
    grid = cells(hour, minute, w, h)
    pal, n = design.PALETTE, design.PALETTE_SIZE
    img = Image.new("RGB", (w, h))
    img.putdata([rgb(design.INSIDE if v == 0xFF else pal[v % n]) for v in grid])
    return img


def on_screen(img):
    """The image as the color display shows it."""
    lut = {}
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            c = px[x, y]
            if c not in lut:
                lut[c] = display.on_screen(c)
            px[x, y] = lut[c]
    return out


def save(img, out, name):
    """Saves the exact image and its on-screen version (name-screen)."""
    base, ext = os.path.splitext(name)
    img.save(os.path.join(out, name))
    on_screen(img).save(os.path.join(out, base + "-screen" + ext))


def save_gif(frames, out, name):
    base, ext = os.path.splitext(name)
    for suffix, fs in (("", frames), ("-screen", [on_screen(f) for f in frames])):
        fs[0].save(os.path.join(out, base + suffix + ext), save_all=True,
                   append_images=fs[1:], duration=1000 // FPS, loop=0)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "preview")
    os.makedirs(out, exist_ok=True)
    minutes = [0, 10, 20, 30, 40, 50, 59]

    for platform in ("basalt", "emery", "diorite"):
        tiles = []
        for hour in range(12):
            for minute in minutes:
                tiles.append((face(platform, hour, minute)[0],
                              "%02d:%02d %s" % (hour, minute, design.target(hour)[0][:13])))
        save(sheet(tiles, len(minutes)), out, "hours-%s.png" % platform)

    for mode in design.MODES:
        name = "platforms.png" if mode == "light" else "platforms-%s.png" % mode
        tiles = [(face(p, 21, 37, mode=mode)[0], "%s 21:37" % p) for p in PLATFORMS]
        w = max(t.width for t, _ in tiles)
        h = max(t.height for t, _ in tiles)
        padded = []
        for t, label in tiles:
            canvas = Image.new("RGB", (w, h), (24, 24, 24))
            canvas.paste(t, ((w - t.width) // 2, (h - t.height) // 2))
            padded.append((canvas, label))
        save(sheet(padded, 4), out, name)

    tiles = [(fractal(hour, minute), "%02d:%02d" % (hour, minute))
             for hour in range(12) for minute in minutes]
    save(sheet(tiles, len(minutes)), out, "fractal.png")

    for platform, (hour, minute) in (("emery", (0, 45)), ("basalt", (5, 30))):
        first, state = face(platform, hour, minute, 0)
        frames = [first] + [face(platform, hour, minute, p, state=state)[0]
                            for p in range(1, design.PALETTE_SIZE)]
        frames = [f.resize((f.width * 2, f.height * 2), Image.NEAREST) for f in frames]
        save_gif(frames, out, "cycle-%s.gif" % platform)
    print("wrote previews to", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
