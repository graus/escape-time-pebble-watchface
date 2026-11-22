"""The digits of color watches: centre lines, and their distance fields.

Each digit is a set of strokes (polylines) in a box 1 wide and GLYPH_ASPECT
tall, y down. The watch never draws these lines: it loads, per digit, the
distance from every point near the digit to its nearest stroke, and thickens
the line by thresholding that distance (see src/c/face.c). So the stroke weight
is free to change every frame.

A distance field is sampled every 2 px: sample (i, j) sits on the pixel
(2i - pad, 2j - pad) of the digit's cell, measured at the pixel's center, in
quarter pixels, clamped to 255.
"""

import math

GLYPH_ASPECT = 1.4
GAP = 0.28        # between the cells of a row and between rows, in digit widths
DIST_ONE = 4      # distance units per pixel
DIST_FAR = 255


def _arc(cx, cy, rx, t0, t1, n=32, ry=None):
    """Points on an ellipse arc, angles in degrees (counter-clockwise on screen)."""
    ry = ry or rx
    return [(cx + rx * math.cos(math.radians(t0 + (t1 - t0) * i / n)),
             cy - ry * math.sin(math.radians(t0 + (t1 - t0) * i / n))) for i in range(n + 1)]


def _six():
    return [_arc(0.5, 0.97, 0.38, 0, 360, 40), _arc(1.0, 0.97, 0.88, 180, 108, 16)]


STROKES = {
    0: [_arc(0.5, 0.7, 0.42, 0, 360, 48, 0.63)],
    1: [[(0.22, 0.32), (0.58, 0.07), (0.58, 1.33)]],
    2: [_arc(0.5, 0.44, 0.38, 195, -38) + [(0.1, 1.33), (0.92, 1.33)]],
    3: [_arc(0.48, 0.4, 0.33, 165, -90), _arc(0.48, 1.0, 0.36, 90, -165)],
    4: [[(0.68, 1.33), (0.68, 0.07), (0.06, 0.95), (0.95, 0.95)]],
    5: [[(0.86, 0.07), (0.22, 0.07), (0.16, 0.62)] + _arc(0.5, 0.95, 0.39, 145, -160)],
    6: _six(),
    7: [[(0.06, 0.07), (0.94, 0.07), (0.38, 1.33)]],
    8: [_arc(0.5, 0.37, 0.3, 0, 360, 36), _arc(0.5, 1.0, 0.37, 0, 360, 40)],
    # 9 is 6 turned half a turn.
    9: [[(1 - x, GLYPH_ASPECT - y) for x, y in s] for s in _six()],
}


def _segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    length_sq = dx * dx + dy * dy
    t = 0.0 if length_sq == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def distance_field(digit, width, pad):
    """(columns, rows, bytes) of a digit `width` px wide, `pad` px around it."""
    assert pad % 2 == 0, "samples must land on even pixels"
    height = round(width * GLYPH_ASPECT)
    cols = (width + 2 * pad) // 2 + 1
    rows = (height + 2 * pad) // 2 + 1
    segments = [(ax * width, ay * width, bx * width, by * width)
                for stroke in STROKES[digit] for (ax, ay), (bx, by) in zip(stroke, stroke[1:])]
    out = bytearray()
    for j in range(rows):
        py = 2 * j - pad + 0.5
        for i in range(cols):
            px = 2 * i - pad + 0.5
            d = min(_segment_distance(px, py, *s) for s in segments)
            out.append(min(DIST_FAR, round(d * DIST_ONE)))
    return cols, rows, bytes(out)
