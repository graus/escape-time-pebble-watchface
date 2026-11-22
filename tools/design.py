"""Escape Time's look, in one place: palette, digit weights and dive targets.

tools/build_assets.py writes these into src/c/generated/ for the watch, and
tools/preview.py renders with them, so previews always match the watch.
"""

# Palette steps per second: how fast the bands flow (src/c/main.c reads the
# generated FPS).
FPS = 5

# The palette cycles: an escaping point takes color (iterations + phase) mod
# its length, and the phase advances every frame, so the bands flow outward.
# It is a closed walk through the Pebble's 4x4x4 color cube in which every
# step changes one channel by one level: neighbours always differ (the motion
# flows instead of stepping) and lightness rises to white and falls back:
# deep blue, sky, white, gold, ember, plum. Nothing is black.
PALETTE = [
    0x0000AA, 0x0055AA, 0x0055FF, 0x00AAFF, 0x55AAFF, 0x55FFFF,
    0xAAFFFF, 0xFFFFFF, 0xFFFFAA, 0xFFFF55, 0xFFAA55, 0xFFAA00,
    0xFF5500, 0xAA5500, 0xAA0000, 0xAA0055, 0x550055, 0x5500AA,
]
PALETTE_SIZE = len(PALETTE)

# Color watches: the digits are white and count as part of the set; a black
# keyline parts them from the bands. Points in the set are black, and
# filaments finer than a pixel (neighbours that escape more than LACE_STEP
# iterations apart) are drawn in one steady color instead of confetti.
DIGIT = 0xFFFFFF
KEYLINE = 0x000000
INSIDE = 0x000000
LACE = 0xFFAA00
LACE_STEP = 6

# B&W watches show the same face, dithered: each color becomes a density of
# lit pixels out of 16 in a 4x4 Bayer pattern. Palette entries (and lace)
# take the lightness they have on the color screen, scaled into
# BW_MIN_LEVEL..BW_MAX_LEVEL: the bands keep their rhythm, black stays the
# keyline's and the set's, and solid white stays the digits' (hue tells them
# apart on color screens; on B&W only lightness can).
BW_MIN_LEVEL = 1
BW_MAX_LEVEL = 11

# Digit stroke weights on color watches, as radii in pixels of a digit 50 px
# wide (scaled with the digit). Each pixel's radius is
#     BASE + DEPTH * depth^2 + TIDE * tide^2
# where depth (0..1) is how deep the dive is at that pixel, ranked between
# the DEPTH_LOW and DEPTH_HIGH quantiles of the view's escape counts, and tide
# (0..1) is a triangle wave over half the palette that rides the moving
# phase (exactly: src/c/face.h). So strokes are fine where the view is shallow, bold where it is
# deep or in the set, and swell in waves that travel outward with the colors.
WEIGHT_REFERENCE = 50
WEIGHT_BASE = 1.6
WEIGHT_DEPTH = 4.6
WEIGHT_TIDE = 2.6
DEPTH_LEVELS = 8      # depth is stored in 3 bits
DEPTH_LOW = 0.25
DEPTH_HIGH = 0.97
# Around each digit: the keyline, then HALO px of bands HALO_BAND px wide that
# flow outward from the digit as the set's do.
KEYLINE_WIDTH = 1.4
HALO = 7.0
HALO_BAND = 1.5

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


def lightness(rgb):
    """A color's perceived lightness (0..1) on the color screen: its
    sRGB-encoded relative luminance, so mid-tones come out mid-gray."""
    import display  # noqa: E402 (tools/ is on the path wherever design is)
    r, g, b = display.on_screen(((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255))
    linear = [(v / 255) ** 2.2 for v in (r, g, b)]
    luminance = 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]
    return luminance ** (1 / 2.2)


def bw_level(rgb):
    """Lit pixels out of 16 for a band's color (palette or lace) on B&W."""
    return round(BW_MIN_LEVEL + (BW_MAX_LEVEL - BW_MIN_LEVEL) * lightness(rgb))


def bw_levels():
    """B&W level of each palette entry."""
    return [bw_level(c) for c in PALETTE]


def bw_face_levels():
    """B&W levels of the digit, keyline, inside and lace (as FaceColors)."""
    return (16, 0, 0, bw_level(LACE))


def is_pebble_color(rgb):
    return all(((rgb >> s) & 255) % 85 == 0 for s in (16, 8, 0))


def argb8(rgb):
    """GColor8 byte: 0b11rrggbb."""
    r, g, b = (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255
    return 0xC0 | (r // 85) << 4 | (g // 85) << 2 | (b // 85)


# The distance field is sampled every 2 px, and interpolated distances
# overshoot by up to 1 px near a centre line, so thinner strokes would break.
MIN_BASE = 1.5


def style(width):
    """Stroke weights for digits `width` px wide, in quarter pixels.

    Returns (base, depth, tide, keyline, halo, halo_band), the FaceStyle of
    src/c/face.h.
    """
    q = width / WEIGHT_REFERENCE
    px = (max(MIN_BASE, WEIGHT_BASE * q), WEIGHT_DEPTH * q, WEIGHT_TIDE * q,
          KEYLINE_WIDTH * q, HALO * q, HALO_BAND * q)
    return tuple(max(1, round(4 * v)) for v in px)


def pad(width):
    """Margin around a digit's cell that its distance field must cover: the
    widest stroke, keyline and halo, rounded up to an even pixel."""
    base, depth, tide, key, halo, _ = style(width)
    px = (base + depth + tide + key + halo + 3) // 4 + 1
    return px + px % 2


def _one_step(a, b):
    """True if b is a's neighbour in the color cube: one channel, one level."""
    steps = [abs(((a >> s) & 255) - ((b >> s) & 255)) // 85 for s in (16, 8, 0)]
    return sorted(steps) == [0, 0, 1]


assert all(is_pebble_color(c) for c in PALETTE + [DIGIT, KEYLINE, INSIDE, LACE])
assert len(set(PALETTE)) == PALETTE_SIZE
assert all(_one_step(a, b) for a, b in zip(PALETTE, PALETTE[1:] + PALETTE[:1]))
assert PALETTE_SIZE % 2 == 0, "the tide is a wave over half the palette"
