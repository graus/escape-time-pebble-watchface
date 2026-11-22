#include "dive.h"

#include "generated/targets.h"  // s_targets: DIVE_TARGETS Misiurewicz points

// The whole set, which spans about -2..0.47 by -1.12..1.12, with a margin.
#define HOME_R MANDEL_FIX(-0.75)
#define HOME_I 0
#define HOME_W MANDEL_FIX(2.9)
#define HOME_H MANDEL_FIX(2.5)

// Extra fraction bits while zooming, so 59 steps of 5/6 do not round away.
// (Multiplied in, not shifted: shifting a negative value left is undefined.)
#define EXTRA 16
#define WIDEN(v) ((int64_t)(v) * ((int64_t)1 << EXTRA))
#define NARROW(v) ((int32_t)((v) / ((int64_t)1 << EXTRA)))

// Iterations: deeper views need more before their detail escapes.
#define BASE_ITER 64
#define ITER_PER_MINUTE 6

static int64_t zoom(int64_t v, int minute) {
  for (int m = 0; m < minute; m++) {
    v = v * 5 / 6;
  }
  return v;
}

void dive_view(MandelView *view, int hour, int minute, int w, int h) {
  const int32_t *t = s_targets[hour % DIVE_TARGETS];
  // Odd hours dive into the mirror image, so the shapes turn the other way.
  int32_t tr = t[0], ti = (hour & 1) ? -t[1] : t[1];
  if (minute < 0) minute = 0;
  if (minute > 59) minute = 59;
  // The step that fits the whole set at :00, whichever side is tighter.
  int64_t step_w = HOME_W / (w > 0 ? w : 1), step_h = HOME_H / (h > 0 ? h : 1);
  int64_t step = step_w > step_h ? step_w : step_h;
  // Zooming about the target: the center's offset from it shrinks with the
  // step, so the target keeps its place in the window.
  view->step = NARROW(zoom(WIDEN(step), minute));
  view->cr = tr + NARROW(zoom(WIDEN(HOME_R - tr), minute));
  view->ci = ti + NARROW(zoom(WIDEN(HOME_I - ti), minute));
  view->max_iter = BASE_ITER + ITER_PER_MINUTE * minute;
}
