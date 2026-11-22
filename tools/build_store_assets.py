#!/usr/bin/env python3
"""Generate Escape Time's app store images, rendered with the watch's own code.

    python3 tools/build_store_assets.py

Every image is real watch frames from tools/preview.py (the watch's C code
compiled for the host), never a mock-up. Outputs:

  store/screenshots/<platform>_timelapse.gif   an hour's dive in a few seconds,
                                               at the platform's exact screen size
  store/screenshots/<platform>_screenshot.png  one moment, same size
  store/screenshots/<platform>_screenshot_dark.png  the same moment in dark mode
  store/icon-80.png, store/icon-144.png        the face in miniature
  store/banner-720x320.png                     the store page banner
  docs/timelapse.gif                           the README's timelapse (emery, 2x)

The timelapse is one frame per minute, :00 to :59, of one hour's dive: at
:00 the whole set, then 1.2x deeper each frame, the digits counting the
minutes and the palette flowing as on the watch (TIMELAPSE_PHASE steps per
frame). Color watches are shown as their screen shows them (muted; see
tools/display.py), like `pebble screenshot`.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FONT = os.path.join(HERE, "fonts", "Rubik-Black.ttf")  # banner text (OFL)
sys.path.insert(0, HERE)
import preview  # noqa: E402

OUT = os.path.join(ROOT, "store")
SHOTS = os.path.join(OUT, "screenshots")

# The dive the store shows: hour 4 dives into the Northwest star M(6,1), the
# richest all the way down. Shown on a 24-hour clock as 16:00-16:59 (same
# dive: 12 targets repeat twice a day).
TIMELAPSE_HOUR = 16
TIMELAPSE_PHASE = 2        # palette steps per frame: the colors flow as it dives
FRAME_MS = 110             # per minute: an hour in under 7 seconds
HOLD_FIRST_MS = 900        # linger on the whole set
HOLD_LAST_MS = 1100        # and on the deepest view
STILL = (16, 37)           # the screenshot's moment

# The store rejects any other size (see store/publish.sh).
SIZES = {"aplite": (144, 168), "basalt": (144, 168), "diorite": (144, 168),
         "flint": (144, 168), "chalk": (180, 180), "emery": (200, 228),
         "gabbro": (260, 260)}


def frame(platform, hour, minute, phase, state=None, mode="light"):
    img, state = preview.face(platform, hour, minute, phase, clock24=True, state=state,
                              mode=mode)
    if preview.PLATFORMS[platform][3]:  # color: as the screen shows it
        img = preview.on_screen(img)
    return img, state


def timelapse(platform, hour=TIMELAPSE_HOUR):
    frames = []
    for minute in range(60):
        img, _ = frame(platform, hour, minute, (minute * TIMELAPSE_PHASE) % preview.design.PALETTE_SIZE)
        frames.append(img)
    durations = [FRAME_MS] * len(frames)
    durations[0], durations[-1] = HOLD_FIRST_MS, HOLD_LAST_MS
    return frames, durations


def pixels_of(img):
    get = getattr(img, "get_flattened_data", None)  # Pillow 12+; getdata goes in 14
    return get() if get else img.getdata()


def to_palette(frames):
    """Frames as P images over one shared exact palette (no dithering: the
    watch has at most 64 colors, so every color fits)."""
    pixels = [list(pixels_of(f)) for f in frames]
    colors = sorted({c for px in pixels for c in px})
    assert len(colors) <= 256, len(colors)
    index = {c: i for i, c in enumerate(colors)}
    flat = [v for c in colors for v in c]
    out = []
    for f, px in zip(frames, pixels):
        p = Image.new("P", f.size)
        p.putpalette(flat)
        p.putdata([index[c] for c in px])
        out.append(p)
    return out


def save_gif(frames, durations, path, scale=1):
    if scale != 1:
        frames = [f.resize((f.width * scale, f.height * scale), Image.NEAREST) for f in frames]
    ps = to_palette(frames)
    ps[0].save(path, save_all=True, append_images=ps[1:], duration=durations, loop=0,
               optimize=False, disposal=1)
    return os.path.getsize(path)


def icon(size):
    """The Mandelbrot set itself, in the face's palette: the whole set, as
    every hour's dive starts. Rendered at 2x with the watch's engine and
    scaled down, so its edges stay smooth at 80 px."""
    img = preview.on_screen(preview.fractal(TIMELAPSE_HOUR, 0, 2 * size, 2 * size))
    return img.resize((size, size), Image.LANCZOS)


def disc(platform, minute, diameter):
    """A round watch's face at a minute of the store's dive, scaled."""
    img, _ = frame(platform, TIMELAPSE_HOUR, minute, (minute * TIMELAPSE_PHASE) % 18)
    img = img.resize((diameter, diameter), Image.LANCZOS)
    mask = Image.new("L", (diameter * 4, diameter * 4), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, diameter * 4 - 1, diameter * 4 - 1), fill=255)
    return img, mask.resize((diameter, diameter), Image.LANCZOS)


def banner(width=720, height=320):
    """The name, and the dive in three moments (:00, :25, :50) on the round
    Pebble Round 2 face, growing as it goes deeper."""
    out = Image.new("RGB", (width, height), (0, 0, 0))
    d = ImageDraw.Draw(out)
    name_font = ImageFont.truetype(FONT, 50)
    tag_font = ImageFont.truetype(FONT, 19)
    x, y = 34, 74
    for word in ("Escape", "Time"):
        box = d.textbbox((0, 0), word, font=name_font)
        d.text((x - box[0], y - box[1]), word, font=name_font, fill=(255, 255, 255))
        y += box[3] - box[1] + 12
    y += 18
    for line in ("A dive into the", "Mandelbrot set,", "every hour"):
        box = d.textbbox((0, 0), line, font=tag_font)
        d.text((x - box[0], y - box[1]), line, font=tag_font, fill=(255, 170, 85))
        y += box[3] - box[1] + 9
    # Three faces, left to right, larger as the dive deepens.
    sizes = ((0, 128), (25, 150), (50, 176))
    gap = 10
    cx = width - 14 - sum(d for _, d in sizes) - gap * (len(sizes) - 1)
    for minute, diameter in sizes:
        img, mask = disc("gabbro", minute, diameter)
        out.paste(img, (cx, (height - diameter) // 2), mask)
        cx += diameter + gap
    assert cx - gap <= width
    return out


def main():
    os.makedirs(SHOTS, exist_ok=True)
    for platform, size in SIZES.items():
        frames, durations = timelapse(platform)
        assert frames[0].size == size, (platform, frames[0].size)
        n = save_gif(frames, durations, os.path.join(SHOTS, "%s_timelapse.gif" % platform))
        still, state = frame(platform, *STILL, phase=4)
        still.save(os.path.join(SHOTS, "%s_screenshot.png" % platform), optimize=True)
        dark, _ = frame(platform, *STILL, phase=4, state=state, mode="dark")
        dark.save(os.path.join(SHOTS, "%s_screenshot_dark.png" % platform), optimize=True)
        print("%-8s timelapse %d frames, %.0f KB" % (platform, len(frames), n / 1024))
    frames, durations = timelapse("emery")
    n = save_gif(frames, durations, os.path.join(ROOT, "docs", "timelapse.gif"), scale=2)
    print("docs/timelapse.gif %.0f KB" % (n / 1024))
    icon(144).save(os.path.join(OUT, "icon-144.png"), optimize=True)
    icon(80).save(os.path.join(OUT, "icon-80.png"), optimize=True)
    banner().save(os.path.join(OUT, "banner-720x320.png"), optimize=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
