// Host-side helpers for tools/preview.py (loaded with ctypes): the watch's
// own mandel.c and dive.c, over a whole window at once.

#include "../../src/c/dive.h"

// Escape counts (or MANDEL_INSIDE) for every cell of a w x h view.
void render_view(const MandelView *view, int w, int h, uint16_t *out) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int32_t cr, ci;
      dive_point(view, x, y, w, h, &cr, &ci);
      out[y * w + x] = mandel_escape(cr, ci, view->max_iter);
    }
  }
}

// The view for a time, exactly as the watch computes it.
void render_time(int hour, int minute, int w, int h, MandelView *view, uint16_t *out) {
  dive_view(view, hour, minute, w, h);
  render_view(view, w, h, out);
}
