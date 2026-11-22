#pragma once
#include <pebble.h>

// Two digit sets: large fills the screen; compact fits above Timeline Quick
// View, which covers the bottom of the screen.
typedef enum {
  DigitSetLarge = 0,
  DigitSetCompact = 1,
} DigitSet;

typedef struct {
  GRect digits[4];  // HH over MM: 0, 1 on the top row, 2, 3 below
} Layout;

// The set to use in an unobstructed area of this height: large when its
// block fits, else compact.
DigitSet layout_choose_set(int area_h);

// The four digit cells (all one size per set, from generated/glyphs.h) on
// even pixels, as their distance fields are sampled every 2 px. The block is
// centered across the screen and in the unobstructed area.
void layout_compute(Layout *out, DigitSet set, GRect area, int screen_w);
