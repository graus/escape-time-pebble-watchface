# Escape Time

A Pebble watch face: the time in four large digits and a dive into the
Mandelbrot set. The digits are part of the set: white strokes whose bands
flow outward with the fractal's, fine where the dive is shallow, bold where
it is deep, swelling in waves as the colors move. B&W watches show the same
face, dithered. The name is the algorithm's: the escape-time algorithm
colors each point by how many steps it takes to escape.

- **Every hour is a new dive, unique to your watch:** each watch makes its
  own random seed, and each hour of each day draws from it a target (one of
  71 spots on the set's edge, or its mirror image) and a spin. No two
  watches, days or hours share a dive.
- **The minute sets how deep:** at :00 the whole set, upright, then 1.2x
  deeper every minute, to about 47,000x at :59, turning a little more each
  minute (the hour's spin plus a random nudge), so the dive spirals in.
- **It moves by color cycling:** the view is computed once a minute and the
  palette flows through it. The digits' outline moves with it.
- **It holds still to spare the battery:** frames run only while a new
  minute's view is computed (it sharpens in, then stops) and for 30 seconds
  after a tap: a flick of the wrist, or a touch on Pebble Time 2 and Round 2.
  While still, nothing runs but the minute tick. Painting a frame takes
  11.4 ms on a real watch, so animating nonstop at 5 fps would keep the CPU
  busy about 6% of the time, plus the display updates.
- **Dark mode** (a setting): black digits made of the same black as the
  set, outlined in white, over a darker palette (navy, violet, plum,
  crimson, ember; no white). *Dark at night* turns it on between set hours
  (22:00-07:00 by default); the face changes on the hour.

An hour on the watch, one frame per minute (09:00-09:59 on one watch, on one
day; every frame is the watch's own rendering, and every watch dives its own
way):

![An hour's dive, one frame per minute](docs/timelapse.gif)

| basalt | emery | gabbro | diorite |
|---|---|---|---|
| ![basalt](docs/screenshots/basalt.png) | ![emery](docs/screenshots/emery.png) | ![gabbro](docs/screenshots/gabbro.png) | ![diorite](docs/screenshots/diorite.png) |

The emery and gabbro images are rendered by `tools/preview.py`; basalt and
diorite are emulator screenshots, which match their previews pixel for
pixel. See
[docs/design](docs/design/README.md) for the design brief.

## How it works

- **Fixed point.** Pebble apps build for a Cortex-M3 without hardware
  floating point, so `src/c/mandel.c` iterates in Q4.27 (int32, 27 fraction
  bits) with 64-bit products. That resolution (7.5e-9) still leaves dozens
  of units per pixel at the deepest minute, which is why the dive is an hour
  of 1.2x steps. A cardioid/bulb test and Brent's cycle check (cycles are
  exact in fixed point) end points inside the set early.
- **Targets with endless detail.** Every dive heads for a Misiurewicz point
  (a c whose critical orbit lands on a cycle after a few steps): the set is
  self-similar around them, so a dive never ends in a flat interior.
  `tools/find_targets.py` finds 690 of them (preperiod + period up to 10);
  `tools/choose_targets.py` dives into each for a whole hour with the
  watch's engine and keeps the 71 that stay good every minute from :10 to
  :59 (no big black blobs, never flat, no confetti deep down), at least 0.01
  apart.
- **Random, but reproducible.** The watch makes its own seed once, from the
  moment the face first runs, and keeps it. Each hour's seed mixes it with
  the date and hour (`dive_hour_seed`, MurmurHash3's finalizer); xorshift32
  then draws the target, whether to mirror it, the hour's spin (up to 4
  degrees a minute either way) and each minute's nudge (up to 2). A view
  depends only on that seed and the minute, so the watch computes it
  directly after a restart, and the host reproduces any watch's dive from
  its seed: the previews match the emulator pixel for pixel.
- **Zooming about the target.** The view's center approaches the target as
  the step shrinks (`src/c/dive.c`), so the target stays fixed on the
  screen while the view turns about the center (a quarter-wave sine table,
  so the watch needs no floating point).
- **Spread over frames.** A new minute's view is computed a row at a time
  within a 40 ms budget per frame, interlaced (every 8th row first), so it
  sharpens evenly over the old one (`src/c/grid.c`). Measured in the basalt
  emulator: 130-250 ms per view of the whole screen.
- **Digits as distance fields.** Each digit is a few centre lines
  (`tools/glyphs.py`). The watch stores, per digit, the distance from every
  point nearby to those lines, sampled every 2 px; a pixel is digit when its
  (bilinearly interpolated) distance is under its stroke radius. The radius
  comes from the pixel's cell: its depth (where its escape count ranks in
  the view) and a triangle wave riding the palette phase. Both are looked up
  per frame in 256-entry tables (`src/c/face.c`), so the shape changes every
  frame for a subtraction and a few compares per pixel. Past the stroke: a
  black keyline, then bands that flow out of the digit.
- **Lace.** Deep in the dive, filaments are finer than a pixel and their
  colors turn to confetti. A cell whose escape count jumps by more than 6
  from its left neighbour's is drawn in one steady color instead.
- **B&W.** The same shading gives each pixel a level instead of a color: the
  palette's lightness on the color screen, scaled to 1-11 of 16 so solid
  white stays the digits', dithered with a 4x4 Bayer pattern
  (`src/c/paint.c`).
- **Settings.** The phone's settings page (`src/pkjs/index.js`) is a small
  HTML page opened as a data URL, with no library or server: Clay, the usual
  one, has watch-side code that does not build for flint or gabbro. Saving
  sends the values as message keys; the watch keeps them (`persist`), so the
  face needs the phone only to change them. A look is a palette and four
  colors (`MODES` in `tools/design.py`, `FaceColors` on the watch); the
  dive and the digits' shapes stay the same, so switching costs nothing.
- **Memory.** One byte per cell of the screen: 1 px cells (24 KB on basalt,
  46 KB on emery, 68 KB on gabbro) plus the distance field (6 KB on basalt).
  Aplite's 24 KB only fits 2 px cells (6 KB); the digits stay sharp, as
  their distances are per pixel.

The layout and Quick View handling come from
[Hearth](https://github.com/graus/hearth-pebble-watchface).

## Building and previewing

    python3 tools/build_assets.py     # digits, palette, targets, sine table, icon
    pebble build
    make -C tools/host test           # the engine on the host
    python3 tools/preview.py          # faces at any time, light and dark, into preview/
    python3 tools/build_store_assets.py  # store timelapses, stills, icons, banner
    store/publish.sh                  # publish to the Pebble app store (goes live)

`tools/design.py` holds the palette, B&W levels, stroke weights and
targets; `tools/glyphs.py` the digit shapes. `tools/preview.py`
renders with the watch's own C code (compiled for the host), so previews
match the watch pixel for pixel, at any time of day; `-screen` images
simulate the color display's muted gamut (`tools/display.py`).

## License

MIT.
