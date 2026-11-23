#!/usr/bin/env python3
"""Choose the dive targets: Misiurewicz points that make a good hour.

    python3 tools/find_targets.py 10 > /tmp/candidates.txt
    python3 tools/choose_targets.py /tmp/candidates.txt > /tmp/targets.py

Every candidate is dived into for a whole hour with the watch's own engine
(tools/preview.py's library), on a 72 x 84 screen, unrotated, and kept only
if every minute from :10 to :59 stays good:

  - at most MAX_INSIDE of the view inside the set (no big black blobs),
  - at least MIN_ENTROPY bits of color bands (the view is not flat),
  - from :40 at most MAX_LACE of the cells on a filament finer than a cell
    (deep views stay structured instead of turning to confetti).

Kept points closer than MIN_APART to a better one are dropped: their dives
would look alike until late in the hour. Mirror images are left out (the
watch mirrors at random). Prints the TARGETS list for tools/design.py.
"""

import collections
import ctypes
import math
import re
import sys

sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import preview  # noqa: E402

W, H = 72, 84
MINUTES = (10, 15, 20, 30, 40, 50, 59)
DEEP = (40, 50, 59)
MAX_INSIDE = 0.45
MIN_ENTROPY = 3.3
MAX_LACE = 0.30
MIN_APART = 0.01
LACE_STEP = 6


def fix(d):
    return int(round(d * (1 << 27)))


def view(tr, ti, minute):
    """The watch's view of a target at a minute, unrotated (src/c/dive.c)."""
    zoom = (5 / 6) ** minute
    step = int(fix(2.9) // min(W, H) * zoom)
    return preview.View(fix(tr + (-0.75 - tr) * zoom), fix(ti - ti * zoom), step, step, 0,
                        64 + 6 * minute)


def counts(tr, ti, minute):
    buf = (ctypes.c_uint16 * (W * H))()
    v = view(tr, ti, minute)
    preview.LIB.render_view(ctypes.byref(v), W, H, buf)
    return list(buf)


def score(c):
    """(share inside, entropy of the bands, share of lace cells)."""
    inside = sum(1 for x in c if x == 0xFFFF) / len(c)
    escaping = [x for x in c if x != 0xFFFF]
    bands = collections.Counter(x % 18 for x in escaping)
    n = len(escaping) or 1
    entropy = -sum(k / n * math.log2(k / n) for k in bands.values())
    lace = 0
    for y in range(H):
        for x in range(1, W):
            a, b = c[y * W + x], c[y * W + x - 1]
            if (a == 0xFFFF) != (b == 0xFFFF) or (a != 0xFFFF and abs(a - b) > LACE_STEP):
                lace += 1
    return inside, entropy, lace / (H * (W - 1))


def main():
    candidates = []
    for line in open(sys.argv[1]):
        m = re.match(r"M\((\d+),(\d+)\)\s+([+-][\d.]+) ([+-][\d.]+)i", line)
        if m and float(m[4]) >= -1e-12:  # one of each mirror pair
            candidates.append(("M(%s,%s)" % (m[1], m[2]), float(m[3]), abs(float(m[4]))))
    kept = []
    for name, r, i in candidates:
        scores = {minute: score(counts(r, i, minute)) for minute in MINUTES}
        worst_inside = max(s[0] for s in scores.values())
        worst_entropy = min(s[1] for s in scores.values())
        deep_lace = max(scores[minute][2] for minute in DEEP)
        if worst_inside <= MAX_INSIDE and worst_entropy >= MIN_ENTROPY and deep_lace <= MAX_LACE:
            kept.append((deep_lace, name, r, i))
    kept.sort()
    chosen = []
    for row in kept:  # least confetti first
        if all(abs(complex(row[2], row[3]) - complex(c[2], c[3])) >= MIN_APART for c in chosen):
            chosen.append(row)
    print("# %d of %d candidates; chosen by tools/choose_targets.py" % (len(chosen), len(candidates)))
    print("TARGETS = [")
    for _, name, r, i in chosen:
        print('    ("%s", %.15f, %.15f),' % (name, r, i))
    print("]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
