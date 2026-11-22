#pragma once
#include <pebble.h>

#include "grid.h"

// Paints the fractal into the white pixels of the digits already drawn in
// fb, over `window` (the digit block's box). `phase` cycles the palette:
// each frame it moves by one, and the bands flow outward from the set.
// Color watches map each cell to a palette color; B&W watches dither it
// with a 4x4 Bayer pattern.
void paint(GBitmap *fb, const Grid *grid, GRect window, uint8_t phase);
