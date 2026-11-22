#include "layout.h"
#include "generated/glyphs.h"

DigitSet layout_choose_set(int area_h) {
  return area_h >= GLYPH_L_FIT_H ? DigitSetLarge : DigitSetCompact;
}

void layout_compute(Layout *out, DigitSet set, GRect area, int screen_w) {
  const bool large = set == DigitSetLarge;
  const int w = large ? GLYPH_L_WIDTH : GLYPH_S_WIDTH;
  const int h = large ? GLYPH_L_HEIGHT : GLYPH_S_HEIGHT;
  const int gap = large ? GLYPH_L_GAP : GLYPH_S_GAP;
  // Even steps keep the right column and bottom row on the sample grid too.
  const int step_x = (w + gap + 1) & ~1, step_y = (h + gap + 1) & ~1;
  const int left = ((screen_w - (step_x + w)) / 2) & ~1;
  const int top = (area.origin.y + (area.size.h - (step_y + h)) / 2) & ~1;
  for (int i = 0; i < 4; i++) {
    out->digits[i] = GRect(left + (i % 2) * step_x, top + (i / 2) * step_y, w, h);
  }
}
