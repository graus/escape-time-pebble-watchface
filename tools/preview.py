#!/usr/bin/env python3
"""Render Escape Time faces on the host, exactly as the watch computes them.

    python3 tools/preview.py [out_dir]     (default: preview/)

The fractal comes from the watch's own src/c/mandel.c, dive.c and grid
logic, compiled for the host and called through ctypes; the digits are the
real resources, laid out as src/c/layout.c does; colors come from
tools/design.py, as src/c/generated/palette.h does. So a preview is the
watch face, pixel for pixel, at any time of day (the emulator only shows
the current time).

Every image comes in two versions: exact Pebble colors, and `-screen`, as
the color display shows them (muted; see tools/display.py). Judge the look
by the -screen versions.

Writes:
  hours-<platform>.png   all 12 dives at :00, :10, ... :59 (rows: hours 0-11)
  platforms.png          one moment on all seven watches
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

FPS = 10  # as src/c/main.c

# name: (width, height, round, color, digit tag)
PLATFORMS = {
    "aplite": (144, 168, False, False, "144w"),
    "basalt": (144, 168, False, True, "144w"),
    "chalk": (180, 180, True, True, "180w"),
    "diorite": (144, 168, False, False, "144w"),
    "emery": (200, 228, False, True, "200w"),
    "flint": (144, 168, False, False, "144w"),
    "gabbro": (260, 260, True, True, "260w"),
}

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
INSIDE = 0xFFFF


class View(ctypes.Structure):
    _fields_ = [("cr", ctypes.c_int32), ("ci", ctypes.c_int32),
                ("step", ctypes.c_int32), ("max_iter", ctypes.c_uint16)]


def load_lib():
    lib_path = os.path.join(ROOT, "tools", "host", "librender.so")
    sources = [os.path.join(ROOT, p) for p in
               ("tools/host/render.c", "src/c/mandel.c", "src/c/dive.c")]
    newest = max(os.path.getmtime(p) for p in sources + [
        os.path.join(ROOT, "src/c/generated/targets.h"),
        os.path.join(ROOT, "src/c/mandel.h"), os.path.join(ROOT, "src/c/dive.h")])
    if not os.path.exists(lib_path) or os.path.getmtime(lib_path) < newest:
        subprocess.check_call(["cc", "-O2", "-shared", "-fPIC", "-Wall", "-Wextra",
                               "-o", lib_path] + sources)
    return ctypes.CDLL(lib_path)


LIB = load_lib()


def metrics(tag_width):
    text = open(os.path.join(ROOT, "src/c/generated/metrics.h")).read()
    block = re.search(r"PBL_DISPLAY_WIDTH == %d\n(.*?)#(?:el|endif)" % tag_width, text, re.S).group(1)
    return {k: int(v) for k, v in re.findall(r"#define DIGIT_(\w+) (\d+)", block)}


def digit(d, tag, compact=False):
    path = os.path.join(ROOT, "resources/digits/%s%d~%s.png" % ("s" if compact else "", d, tag))
    return Image.open(path).convert("1")


def layout(platform, hh, mm):
    """Digit boxes and the window, as src/c/layout.c (large set, full screen)."""
    w, h, _, color, tag = PLATFORMS[platform]
    m = metrics(int(tag[:-1]))
    imgs = [digit(int(c), tag) for c in "%02d%02d" % (hh, mm)]
    height, row_gap, col_gap = m["L_HEIGHT"], m["L_ROW_GAP"], m["L_COL_GAP"]
    spare = h - 2 * height
    row_gap = min(row_gap, max(spare, 0))
    block_h = 2 * height + row_gap
    top = (h - block_h) // 2
    boxes = []
    for row in range(2):
        a, b = imgs[row * 2], imgs[row * 2 + 1]
        row_w = a.width + col_gap + b.width
        x = (w - row_w) // 2
        y = top + row * (height + row_gap)
        boxes += [(x, y, a), (x + a.width + col_gap, y, b)]
    win_w = m["L_MAX_PAIR_WIDTH"] + col_gap
    left = (w - win_w) // 2
    right = left + win_w
    if not color:
        left &= ~7
        right = (right + 7) & ~7
        left = max(left, 0)
        right = min(right, w & ~7)
    return boxes, (left, top, right - left, block_h)


def cells(hour, minute, cw, ch):
    buf = (ctypes.c_uint16 * (cw * ch))()
    view = View()
    LIB.render_time(hour, minute, cw, ch, ctypes.byref(view), buf)
    n = design.PALETTE_SIZE
    return [0xFF if v == INSIDE else v % n for v in buf]


def rgb(c):
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255)


def face(platform, hour, minute, phase=0, clock24=True, grid=None):
    """The face at a time (24-hour `hour` drives the dive)."""
    w, h, is_round, color, _ = PLATFORMS[platform]
    shown = hour if clock24 else (hour % 12 or 12)
    boxes, (left, top, win_w, win_h) = layout(platform, shown, minute)
    shift = 0 if color else 1
    cw, ch = (win_w + (1 << shift) - 1) >> shift, (win_h + (1 << shift) - 1) >> shift
    if grid is None:
        grid = cells(hour, minute, cw, ch)
    mask = Image.new("1", (w, h), 0)
    for x, y, img in boxes:
        mask.paste(img, (x, y))
    out = Image.new("RGB", (w, h), (0, 0, 0))
    px, mp = out.load(), mask.load()
    pal, levels = design.palette(), design.bw_levels()
    n = design.PALETTE_SIZE
    for y in range(max(top, 0), min(top + win_h, h)):
        for x in range(max(left, 0), min(left + win_w, w)):
            if not mp[x, y]:
                continue
            v = grid[((y - top) >> shift) * cw + ((x - left) >> shift)]
            if color:
                px[x, y] = rgb(design.INSIDE if v == 0xFF else pal[(v + phase) % n])
            else:
                level = 16 if v == 0xFF else levels[(v + phase) % n]
                px[x, y] = (255, 255, 255) if BAYER[y & 3][x & 3] < level else (0, 0, 0)
    if is_round:
        circle = Image.new("L", (w, h), 0)
        ImageDraw.Draw(circle).ellipse((0, 0, w - 1, h - 1), fill=255)
        out = Image.composite(out, Image.new("RGB", (w, h)), circle)
    return out, grid


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
    pal, n = design.palette(), design.PALETTE_SIZE
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

    for platform in ("basalt", "emery"):
        tiles = []
        for hour in range(12):
            for minute in minutes:
                tiles.append((face(platform, hour, minute)[0],
                              "%02d:%02d %s" % (hour, minute, design.target(hour)[0][:13])))
        save(sheet(tiles, len(minutes)), out, "hours-%s.png" % platform)

    tiles = [(face(p, 21, 37)[0], "%s 21:37" % p) for p in PLATFORMS]
    w = max(t.width for t, _ in tiles)
    h = max(t.height for t, _ in tiles)
    padded = []
    for t, label in tiles:
        canvas = Image.new("RGB", (w, h), (24, 24, 24))
        canvas.paste(t, ((w - t.width) // 2, (h - t.height) // 2))
        padded.append((canvas, label))
    save(sheet(padded, 4), out, "platforms.png")

    tiles = [(fractal(hour, minute), "%02d:%02d" % (hour, minute))
             for hour in range(12) for minute in minutes]
    save(sheet(tiles, len(minutes)), out, "fractal.png")

    for platform, (hour, minute) in (("emery", (0, 45)), ("basalt", (5, 30))):
        first, grid = face(platform, hour, minute, 0)
        frames = [first] + [face(platform, hour, minute, p, grid=grid)[0]
                            for p in range(1, design.PALETTE_SIZE)]
        frames = [f.resize((f.width * 2, f.height * 2), Image.NEAREST) for f in frames]
        save_gif(frames, out, "cycle-%s.gif" % platform)
    print("wrote previews to", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
