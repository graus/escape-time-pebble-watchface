#pragma once
#include <pebble.h>

#include "face.h"
#include "grid.h"

// Paints the whole face into fb, row by row (see face.h): the frame's
// lookups over the grid, which covers the screen, and the digits' distances.
// Color watches shade straight into the framebuffer (the frame's colors are
// GColor8 bytes); B&W watches shade a row of levels (lit pixels out of 16)
// and dither it with a 4x4 Bayer pattern.
void paint_face(GBitmap *fb, const FaceFrame *frame, const Grid *grid,
                const DistField *dist);
