#pragma once
// From the time to the view: each hour dives into its own target, and the
// minute sets how deep. Plain C with no Pebble dependency.
//
// At :00 the window shows the whole set. Every minute zooms in by 6/5 about
// the hour's target, so the target stays put while everything grows around
// it: at :59 the zoom is (6/5)^59, about 47,000.

#include "mandel.h"

#define DIVE_TARGETS 12

// The view for an hour (0..23, 12 targets repeat twice a day) and minute
// (0..59), for a window `w` x `h` cells.
void dive_view(MandelView *view, int hour, int minute, int w, int h);

// The complex point of a cell.
static inline void dive_point(const MandelView *v, int x, int y, int w, int h,
                              int32_t *cr, int32_t *ci) {
  *cr = v->cr + (x - w / 2) * v->step;
  *ci = v->ci - (y - h / 2) * v->step;  // imaginary axis points up
}
