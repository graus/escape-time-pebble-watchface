# Escape Time

A Pebble watch face: the time in four large digits, each one a window onto a
dive into the Mandelbrot set. The name is the algorithm's: the escape-time
algorithm colors each point by how many steps it takes to escape.

- **The hour picks where to dive:** twelve targets spread around the set,
  repeated twice a day; odd hours dive into the mirror image.
- **The minute sets how deep:** at :00 the whole set, then 1.2x deeper every
  minute, to about 47,000x at :59.
- **It moves by color cycling:** the view is computed once a minute and the
  palette flows through it, so animating costs almost nothing.

| basalt | emery | gabbro | diorite |
|---|---|---|---|
| ![basalt](docs/screenshots/basalt.png) | ![emery](docs/screenshots/emery.png) | ![gabbro](docs/screenshots/gabbro.png) | ![diorite](docs/screenshots/diorite.png) |

Status: first version, before the design pass. See
[docs/design](docs/design/README.md) for the brief and review images.

## How it works

- **Fixed point.** Pebble apps build for a Cortex-M3 without hardware
  floating point, so `src/c/mandel.c` iterates in Q4.27 (int32, 27 fraction
  bits) with 64-bit products. That resolution (7.5e-9) still leaves dozens
  of units per pixel at the deepest minute, which is why the dive is an hour
  of 1.2x steps. A cardioid/bulb test and Brent's cycle check (cycles are
  exact in fixed point) end points inside the set early.
- **Targets with endless detail.** Each hour dives into a Misiurewicz point
  (a c whose critical orbit lands on a cycle after a few steps), found with
  `tools/find_targets.py`, or Seahorse Valley's classic deep-zoom point. The
  set is self-similar around Misiurewicz points, so a dive never ends in a
  flat interior.
- **Zooming about the target.** The view's center approaches the target as
  the step shrinks (`src/c/dive.c`), so the target stays fixed in the window.
- **Spread over frames.** A new minute's view is computed a row at a time
  within a 40 ms budget per frame, interlaced (every 8th row first), so it
  sharpens evenly over the old one (`src/c/grid.c`). Measured in the basalt
  emulator: about 140 ms per view.
- **Memory.** One byte per cell of the digit block: 1 px cells on color
  watches (about 20 KB on basalt, 40 KB on emery), 2 px cells on B&W ones
  (5 KB on aplite), coarser if the heap is short.

The digits, layout and Quick View handling come from
[Hearth](https://github.com/graus/hearth-pebble-watchface).

## Building and previewing

    python3 tools/build_assets.py     # digits, metrics, palette, targets, icon
    pebble build
    make -C tools/host test           # the engine on the host
    python3 tools/preview.py          # faces at any time, into preview/

`tools/design.py` holds the palette and the targets. `tools/preview.py`
renders with the watch's own C code (compiled for the host), so previews
match the watch pixel for pixel, at any time of day; `-screen` images
simulate the color display's muted gamut (`tools/display.py`).

## License

MIT. Rubik (the digit font) is under the SIL Open Font License
(`tools/fonts/OFL.txt`).
