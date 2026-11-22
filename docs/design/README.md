> **Historical.** This is the brief for the design pass, written against the
> first version. Its outcome is in commits 1db3984 (color: digits drawn from
> the set, with a moving tide) and 950c100 (B&W: the same face, dithered);
> the README describes the face as it is now.

# Escape Time: design brief

A Pebble watch face. The time is four large digits (HH over MM), and each
digit is a window onto a dive into the Mandelbrot set. Everything outside the
digits is black. The images in this folder are the current first version; I
want it to look top-notch.

## How the face works

- **The hour picks where to dive.** Twelve targets spread around the set
  (Seahorse Valley, spirals, branch points), repeated twice a day; odd hours
  dive into the mirror image.
- **The minute sets how deep.** At :00 the window shows the whole set; every
  minute zooms in 1.2x on the hour's target, which stays fixed in the window.
  At :59 the zoom is about 47,000x. At the top of the hour it starts over
  somewhere new.
- **It moves by color cycling.** The fractal is computed once a minute; after
  that only the palette moves. A pixel that escaped after *n* iterations shows
  palette entry `(n + phase) mod 24`, and the phase steps 10 times a second,
  so the bands flow outward from the set. Points inside the set show one
  steady "inside" color.

## Hard constraints (the hardware)

- **64 colors.** Each channel is 0, 85, 170 or 255 (hex `00 55 AA FF`). No
  other colors exist, no alpha, no anti-aliasing.
- **The screen mutes colors.** The color display's gamut is soft: pure
  `#0055AA` shows as roughly `#16638D`. All `-screen` images here are
  simulated with pebble-tool's display table, so judge by those.
- **Screens:** 144x168 (basalt, rectangular), 180x180 (chalk, round), 200x228
  (emery), 260x260 (gabbro, round). Black-and-white watches (aplite, diorite,
  flint, 144x168) are 1-bit: there the palette becomes dither densities.
- **B&W:** each palette entry is a density of 0-16 lit pixels out of 16 in a
  4x4 Bayer pattern, on 2x2-pixel cells. Inside the set is solid white.
- **Digits:** Rubik Black, as large as the screen allows, white on black
  before the fractal is painted into them. The digits must stay readable at a
  glance on a 1.3-inch screen; nothing in the palette is black for that reason.

## What can change (cheaply)

- **The palette:** any number of entries (now 24), any of the 64 colors, in
  any order. It loops, so the last entry flows into the first.
- **The inside color** (now navy `#000055`).
- **B&W densities** per palette entry (now a triangle wave from 5/16 to 16/16).
- **Animation speed** (now 10 steps a second) and direction.
- The background (now black), digit treatment (e.g. an outline), or the
  font, within the memory budget.

## Known problems (please weigh in)

1. **Speckle deep in the dive.** In images 01-03 at :30-:59, the filaments
   are finer than a pixel and read as confetti. Wider bands don't fix it
   (08-speckle-band-test.png). The engineering fix (distance-estimation
   coloring or supersampling) changes what the image can look like: smooth
   gradients toward the set with thin bright filaments. Which look do you want?
2. **Flat moments.** Some minutes show mostly one band or the inside color
   (e.g. 00:10, 02:10): the digits become flat shapes. Is that a calm feature
   or a bug?
3. **The palette barely survives quantization:** the 24 entries collapse to 12
   distinct colors, in runs of up to 5 identical entries, so the motion
   steps rather than flows.
4. **B&W is low-contrast gray** (04-all-watches.png, aplite/diorite/flint).
5. **Muted screen:** the blues and plums go dull on the real display.

## What I'd like back

- A palette (an ordered list of the 64 colors, any length) and an inside
  color, for color watches, and the reasoning for it.
- A density list for B&W watches.
- A view on problems 1-2 and anything else that would make it attractive:
  animation speed, digit treatment, background.

Answers in that form drop straight into `tools/design.py`; the previews are
then regenerated with `python3 tools/preview.py`.

## The images

| File | What |
|---|---|
| 01-emulator-captures.png | Real emulator screenshots (basalt 12:49, the others 12:52) |
| 02-every-hour-basalt.png | All 12 dives at :00, :10 ... :59 on a 144x168 watch |
| 03-every-hour-emery.png | The same on the 200x228 watch |
| 04-all-watches.png | 21:37 on all seven watches, color and B&W |
| 05-raw-fractal.png | The fractal per hour and depth, without the digits |
| 06-motion-emery.gif, 07-motion-basalt.gif | One full palette cycle (the motion) at 2x |
| 08-speckle-band-test.png | 1, 2 and 4 iterations per color step: the speckle stays |

The previews are not mock-ups: they are rendered by the watch's own C code
and match the emulator pixel for pixel (verified on basalt).
