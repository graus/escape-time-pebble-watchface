#pragma once
// The escape-time Mandelbrot iteration, in fixed point. Plain C with no
// Pebble dependency, so the host tools (tests, previews) run the exact code
// the watch runs.
//
// Numbers are Q4.27: int32 with 27 fraction bits, range +-16, resolution
// 2^-27 (7.5e-9). An hour's dive reaches a zoom of 1.2^59 (about 47,000), where
// a pixel still spans dozens of units, so the image stays crisp to the end.

#include <stdint.h>

#define MANDEL_FRAC 27
#define MANDEL_ONE ((int32_t)1 << MANDEL_FRAC)

// Converts a double to Q4.27 (host tools and constant tables only).
#define MANDEL_FIX(d) ((int32_t)((d) * (double)MANDEL_ONE + ((d) < 0 ? -0.5 : 0.5)))

// Returned by mandel_escape for points that never escape (in the set).
#define MANDEL_INSIDE 0xFFFF

// Iterations until z = z^2 + c leaves the radius-2 disc, starting at z = 0,
// or MANDEL_INSIDE if it has not left after max_iter iterations or is caught
// in a cycle (which fixed point arithmetic makes exact, so cheap to detect).
uint16_t mandel_escape(int32_t cr, int32_t ci, uint16_t max_iter);

// The view of one minute: the complex point at the screen's center, the
// distance and direction between neighbouring cells, and the iteration budget.
typedef struct {
  int32_t cr, ci;     // center, Q4.27
  int32_t step;       // distance between neighbouring cells, Q4.27
  int32_t ux, uy;     // one cell to the right, as a complex step (step,
                      // turned by the view's angle), Q4.27
  uint16_t max_iter;
} MandelView;
