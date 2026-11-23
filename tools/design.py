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

# Dark mode (a setting): black digits, made of the same black as the set,
# parted from the bands by a white keyline, which also traces them where they
# cross the set. Its palette is a darker walk by the same rules (and of the
# same length: the grid stores bands modulo it), with no white or pale
# entries: navy, blue, violet, plum, crimson, ember, one glint of gold, then
# maroon back down to the darkest. The inside and lace stay as they are.
DARK_PALETTE = [
    0x000055, 0x0000AA, 0x0055AA, 0x0055FF, 0x5555FF, 0x5500FF,
    0x5500AA, 0xAA00AA, 0xAA0055, 0xFF0055, 0xFF0000, 0xFF5500,
    0xFFAA00, 0xAAAA00, 0xAA5500, 0xAA0000, 0x550000, 0x550055,
]
DARK_DIGIT = 0x000000
DARK_KEYLINE = 0xFFFFFF

# The face's looks, in the order of the watch's FACE_MODE_* and the
# settings page's choices: palette, digit, keyline, inside, lace.
MODES = {
    "light": (PALETTE, DIGIT, KEYLINE, INSIDE, LACE),
    "dark": (DARK_PALETTE, DARK_DIGIT, DARK_KEYLINE, INSIDE, LACE),
}

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

# Dive targets. Each hour's seed (src/c/dive.c) picks one, and whether to
# dive into its mirror image (negative imaginary part). All are Misiurewicz
# points M(k, p), where the set has detail at every depth, found by
# tools/find_targets.py (k + p up to 10) and chosen by tools/choose_targets.py
# for an hour that stays good: no big black blobs, never flat, and no
# confetti deep down. (name, real, imaginary)
# 71 of 690 candidates; chosen by tools/choose_targets.py
TARGETS = [
    ("M(7,1)", -0.012489256325033, 1.029032786918613),
    ("M(9,1)", -0.143996637975694, 1.047264481867250),
    ("M(2,8)", -1.263829922916239, 0.386343910695352),
    ("M(6,1)", -0.016605716921475, 1.006001828834065),
    ("M(2,7)", 0.381986726221449, 0.685123150983724),
    ("M(3,6)", -1.032444627300152, 0.386399349633542),
    ("M(5,2)", -1.255581976753862, 0.402038859265814),
    ("M(2,7)", -0.043200775114963, 0.997523246920852),
    ("M(7,2)", -1.509171750901420, 0.000000000000000),
    ("M(9,1)", -1.480644255205939, 0.018686326533489),
    ("M(4,4)", -0.627286013424069, 0.689703158356453),
    ("M(5,4)", -1.496464687155908, 0.000000000000000),
    ("M(6,4)", -1.255626812785755, 0.327014801858233),
    ("M(7,2)", -1.412230467679666, 0.144332181835607),
    ("M(5,4)", -0.168987114927887, 1.042370253271614),
    ("M(2,8)", -1.011355544733183, 0.372335803427582),
    ("M(2,7)", -0.602141546211731, 0.690869730794004),
    ("M(4,3)", -0.027219872189944, 0.995454534922548),
    ("M(8,1)", -0.137775189806990, 1.014772953490405),
    ("M(2,5)", -0.615805132905564, 0.687211340203275),
    ("M(6,2)", -1.260362981978614, 0.355326475723239),
    ("M(2,8)", -1.050958881167811, 0.360904699958237),
    ("M(3,7)", -1.395460900347957, 0.106193685903085),
    ("M(3,6)", -1.194195773197392, 0.324958579195321),
    ("M(5,5)", -0.249504672119269, 0.887783034119276),
    ("M(9,1)", -0.058198403879567, 0.984768787438784),
    ("M(4,5)", -1.281339175361563, 0.188731669382710),
    ("M(5,4)", -0.527997098121315, 0.706410102617195),
    ("M(4,5)", -0.615478778433645, 0.625934357431392),
    ("M(3,7)", -1.155851067546879, 0.336169180079062),
    ("M(8,2)", -1.404132992534121, 0.125448542820976),
    ("M(5,1)", -1.222863787129900, 0.316882263887136),
    ("M(2,8)", -1.287524697236186, 0.198500091037264),
    ("M(2,8)", -0.917248958077638, 0.323600287345895),
    ("M(3,5)", -1.041195521091176, 0.364042217707010),
    ("M(5,5)", -1.267262759974775, 0.188216702608842),
    ("M(5,3)", -0.134965283182701, 1.000168994892129),
    ("M(5,5)", -1.049918028160696, 0.337130164352157),
    ("M(3,7)", -1.273818551017800, 0.178722214101946),
    ("M(9,1)", -1.153764418559038, 0.324672914170644),
    ("M(3,4)", -1.144365877399032, 0.314719470372147),
    ("M(6,3)", -0.695322780315587, 0.481295013022466),
    ("M(6,4)", -0.708405004917592, 0.476839872575731),
    ("M(6,3)", -0.535689359707768, 0.687190321663234),
    ("M(6,3)", -0.065666895830919, 0.976035220583662),
    ("M(4,1)", -0.101096363845622, 0.956286510809142),
    ("M(5,2)", -1.430357632451308, 0.000000000000000),
    ("M(3,6)", -1.368856149342111, 0.093453522536864),
    ("M(4,4)", -0.266891685287402, 0.856840881605234),
    ("M(3,7)", -0.509708297841342, 0.691713520773439),
    ("M(3,3)", -0.607031022616088, 0.605251381278934),
    ("M(5,4)", -0.984730833412547, 0.313801691563350),
    ("M(3,6)", -0.701359036552529, 0.463087870941078),
    ("M(2,4)", -0.564097854850762, 0.679273348292929),
    ("M(7,1)", -1.030029178289476, 0.326844529784818),
    ("M(5,5)", -0.930346005591584, 0.316247121779144),
    ("M(6,4)", -0.167133618422227, 0.901276231474706),
    ("M(2,7)", -0.916762524996768, 0.307048726479648),
    ("M(5,5)", -0.659189189548267, 0.495045536682584),
    ("M(6,2)", -0.635617028841210, 0.491920415203862),
    ("M(5,3)", -0.696240968614722, 0.438754598050095),
    ("M(8,1)", -0.523319477139541, 0.688133302278425),
    ("M(7,1)", -0.239716190223134, 0.845503313700289),
    ("M(4,6)", -0.646241734866212, 0.497880647993012),
    ("M(9,1)", -0.609992357646681, 0.616406719404251),
    ("M(4,4)", -0.670524811968762, 0.482038738408294),
    ("M(3,6)", -0.980132823584884, 0.297988374318253),
    ("M(7,3)", -0.107940840100337, 0.905913963597454),
    ("M(7,2)", -1.162341599884039, 0.292368933896586),
    ("M(2,6)", -0.679752658792836, 0.473466324280083),
    ("M(6,1)", -0.562202621523037, 0.642817149072775),
]


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


def palette(mode="light"):
    return MODES[mode][0]


def face_colors(mode="light"):
    """The digit, keyline, inside and lace colors (as FaceColors)."""
    return MODES[mode][1:5]


def bw_levels(mode="light"):
    """B&W level of each palette entry. The levels follow lightness, so dark
    mode's palette dithers darker too."""
    return [bw_level(c) for c in palette(mode)]


def bw_face_levels(mode="light"):
    """B&W levels of the digit, keyline, inside and lace (as FaceColors):
    black and white are solid (0 and 16), lace dithers like the bands."""
    def level(rgb):
        return {0x000000: 0, 0xFFFFFF: 16}.get(rgb, bw_level(rgb))
    return tuple(level(c) for c in face_colors(mode))


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


for _p, *_colors in MODES.values():
    assert all(is_pebble_color(c) for c in _p + _colors)
    assert len(_p) == PALETTE_SIZE and len(set(_p)) == PALETTE_SIZE
    assert all(_one_step(a, b) for a, b in zip(_p, _p[1:] + _p[:1]))
assert PALETTE_SIZE % 2 == 0, "the tide is a wave over half the palette"
assert PALETTE_SIZE >= 4, "the tide's crest is 0 below 4 entries (src/c/face.c divides by it)"
