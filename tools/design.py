"""Escape Time's look, in one place: the palette and the dive targets.

tools/build_assets.py writes these into src/c/generated/ for the watch, and
tools/preview.py renders with them, so previews always match the watch.
"""

# The palette cycles: an escaping point takes color (iterations + phase) mod
# its length, and the phase advances every frame, so the bands flow outward
# without recomputing anything. Stops are (position, RGB) on a loop of
# PALETTE_SIZE; the last stop wraps to the first. Each color is rounded to
# the Pebble's 64 (2 bits per channel). Nothing is black, so the digits never
# vanish into the background, and nothing is navy, the inside color.
PALETTE_SIZE = 24
PALETTE_STOPS = [
    (0, 0x0055AA),   # deep blue
    (5, 0x55AAFF),   # sky
    (8, 0xFFFFFF),   # white
    (11, 0xFFAA00),  # gold
    (15, 0xAA5500),  # amber
    (19, 0x550055),  # plum
]
# Points in the set: a steady color under the moving bands.
INSIDE = 0x000055

# B&W watches dither: lit pixels out of 16 for each palette entry (a triangle
# wave between DIM and 16), and solid white inside the set.
BW_DIM = 5

# Dive targets, one per hour (12, repeated twice a day). Each is a
# boundary point with detail at every depth: Misiurewicz points M(k, p) from
# tools/find_targets.py, and Seahorse Valley's classic deep-zoom point. They
# are spread around the set, and odd hours dive into the mirror image
# (negative imaginary part) so the shapes turn the other way up.
# (name, real, imaginary)
TARGETS = [
    ("Seahorse Valley", -0.743643887037151, 0.131825904205330),
    ("North spiral M(4,1)", -0.101096363845622, 0.956286510809142),
    ("East branch M(2,4)", 0.435134661051023, 0.317810231469781),
    ("Antenna M(5,2)", -1.430357632451308, 0.0),
    ("Northwest star M(6,1)", -0.562202621523037, 0.642817149072775),
    ("Northeast star M(5,1)", 0.366362983422764, 0.591533773261445),
    ("West bulb M(3,5)", -1.041195521091176, 0.364042217707010),
    ("Crown M(7,1)", -0.239716190223134, 0.845503313700289),
    ("East filament M(7,1)", 0.424512719050040, 0.207530228166745),
    ("West star M(3,4)", -1.144365877399032, 0.314719470372147),
    ("Shoulder M(3,5)", 0.148986442255543, 0.668322934458817),
    ("Neck M(4,4)", -0.670524811968762, 0.482038738408294),
]


def target(hour):
    """(name, real, imaginary) for an hour, mirrored on odd hours."""
    name, r, i = TARGETS[hour % len(TARGETS)]
    return (name, r, -i if hour % 2 else i)


def lerp_rgb(a, b, t):
    out = 0
    for shift in (16, 8, 0):
        x, y = (a >> shift) & 255, (b >> shift) & 255
        out |= int(round(x + (y - x) * t)) << shift
    return out


def pebble_rgb(rgb):
    """Round each channel to the Pebble's 4 levels (0, 85, 170, 255)."""
    out = 0
    for shift in (16, 8, 0):
        v = (rgb >> shift) & 255
        out |= ((v + 42) // 85 * 85) << shift
    return out


def palette():
    """PALETTE_SIZE Pebble colors (RGB ints), cyclic."""
    stops = PALETTE_STOPS + [(PALETTE_SIZE, PALETTE_STOPS[0][1])]
    pal = []
    for i in range(PALETTE_SIZE):
        for (a, ca), (b, cb) in zip(stops, stops[1:]):
            if a <= i < b:
                pal.append(pebble_rgb(lerp_rgb(ca, cb, (i - a) / (b - a))))
                break
    return pal


def bw_levels():
    """Lit pixels out of 16 per palette entry: a triangle wave, DIM..16."""
    half = PALETTE_SIZE / 2
    return [round(BW_DIM + (16 - BW_DIM) * (1 - abs(i - half) / half))
            for i in range(PALETTE_SIZE)]


def argb8(rgb):
    """GColor8 byte: 0b11rrggbb."""
    r, g, b = (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255
    return 0xC0 | (r // 85) << 4 | (g // 85) << 2 | (b // 85)
