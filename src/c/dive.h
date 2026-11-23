#pragma once
// From the time to the view: every hour a new dive, unique to the watch, the
// day and the hour. Plain C with no Pebble dependency.
//
// An hour's seed (dive_hour_seed) picks its target, one of DIVE_TARGETS
// Misiurewicz points (where the set has detail at every depth, found by
// tools/find_targets.py and checked over a whole hour), and whether to dive
// into its mirror image. At :00 the screen shows the whole set, upright.
// Every minute the view zooms in 6/5 about the target, which keeps its
// place on the screen while everything grows around it, and turns: by the
// hour's random spin plus a random nudge of its own, so the dive spirals in
// and no two minutes look alike. At :59 the zoom is (6/5)^59, about 47,000.
//
// The view depends only on the seed and the minute, so the watch computes it
// directly (no path to replay after a restart) and the host reproduces it.

#include <stdint.h>

#include "mandel.h"

// The seed of one hour's dive, from the watch's own seed and the local date
// and hour: no two watches, days or hours share a dive.
uint32_t dive_hour_seed(uint32_t watch_seed, int year, int yday, int hour);

// The view of an hour's dive (its seed) at a minute (0..59), for a screen
// of w x h cells.
void dive_view(MandelView *view, uint32_t seed, int minute, int w, int h);

// The complex point of a cell: the screen's axes turned by the view's angle.
static inline void dive_point(const MandelView *v, int x, int y, int w, int h,
                              int32_t *cr, int32_t *ci) {
  int dx = x - w / 2, dy = y - h / 2;
  *cr = v->cr + dx * v->ux + dy * v->uy;
  *ci = v->ci + dx * v->uy - dy * v->ux;  // imaginary axis points up
}
