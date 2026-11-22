#include "mandel.h"

// 4 in the Q8.54 of a product of two Q4.27 numbers.
#define ESCAPE_RADIUS_SQ ((int64_t)4 << (2 * MANDEL_FRAC))

// The main cardioid and the period-2 disc hold most of the set's area at the
// top of the hour; testing for them skips max_iter iterations per pixel.
static int in_main_shapes(int32_t cr, int32_t ci) {
  int64_t yy = ((int64_t)ci * ci) >> MANDEL_FRAC;
  // Period-2 disc: (x + 1)^2 + y^2 < 1/16.
  int64_t x1 = (int64_t)cr + MANDEL_ONE;
  if (((x1 * x1) >> MANDEL_FRAC) + yy < (MANDEL_ONE >> 4)) {
    return 1;
  }
  // Main cardioid: q (q + (x - 1/4)) < y^2 / 4, q = (x - 1/4)^2 + y^2.
  int64_t xq = (int64_t)cr - (MANDEL_ONE >> 2);
  int64_t q = ((xq * xq) >> MANDEL_FRAC) + yy;
  return ((q * (q + xq)) >> MANDEL_FRAC) < (yy >> 2);
}

uint16_t mandel_escape(int32_t cr, int32_t ci, uint16_t max_iter) {
  if (in_main_shapes(cr, ci)) {
    return MANDEL_INSIDE;
  }
  int32_t x = 0, y = 0;
  // Brent's cycle check: compare z with a saved z, re-saving at powers of 2.
  int32_t saved_x = 0, saved_y = 0;
  uint16_t lap = 8, lap_left = 8;
  for (uint16_t i = 0; i < max_iter; i++) {
    int64_t xx = (int64_t)x * x;
    int64_t yy = (int64_t)y * y;
    if (xx + yy > ESCAPE_RADIUS_SQ) {
      return i;
    }
    // Before the check |z| <= 2, so after it |z| <= 4 + |c| < 16: no overflow.
    int64_t xy = (int64_t)x * y;
    y = (int32_t)(xy >> (MANDEL_FRAC - 1)) + ci;  // 2xy + ci
    x = (int32_t)((xx - yy) >> MANDEL_FRAC) + cr;
    if (x == saved_x && y == saved_y) {
      return MANDEL_INSIDE;
    }
    if (--lap_left == 0) {
      saved_x = x;
      saved_y = y;
      lap = lap < 0x4000 ? lap * 2 : lap;
      lap_left = lap;
    }
  }
  return MANDEL_INSIDE;
}
