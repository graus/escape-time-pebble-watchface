#pragma once
#include <pebble.h>

#include "grid.h"

#if defined(PBL_COLOR)
#include "face.h"

// Paints the whole face into fb, row by row (see face.h): the frame's
// lookups over the grid (which covers the screen) and the digits' distances.
void paint_face(GBitmap *fb, const FaceFrame *frame, const Grid *grid,
                const DistField *dist);

#else

// Paints the fractal into the white pixels of the digits already drawn in
// fb, over `window` (the digit block's box), dithered with a 4x4 Bayer
// pattern. `phase` cycles the palette: each frame it moves by one, and the
// bands flow outward from the set.
void paint(GBitmap *fb, const Grid *grid, GRect window, uint8_t phase);

#endif
