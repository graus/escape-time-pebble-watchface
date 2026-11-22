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
  GRect window;     // B&W: the digit block's bounding box, where the dive shows
} Layout;

// The set to use in an unobstructed area of this height: large when its
// block fits, else compact.
DigitSet layout_choose_set(int area_h);

#if defined(PBL_COLOR)
// Color watches: the four digit cells (all one size per set, from
// generated/glyphs.h) on even pixels, as their distance fields are sampled
// every 2 px. The block is centered across the screen and in the
// unobstructed area. `window` is the whole screen: the dive fills it.
void layout_compute_glyphs(Layout *out, DigitSet set, GRect area, GSize screen);
#endif

// B&W watches: places the four digits (given their bitmap sizes, all from `set`) in the
// unobstructed area. Rows are centered by their real width; the block is
// centered vertically. The flame window is derived from the set's worst-case
// metrics, so it does not change from minute to minute (only with the set or
// the unobstructed area).
void layout_compute(Layout *out, DigitSet set, GRect area, int screen_w,
                    const GSize digit_sizes[4]);
