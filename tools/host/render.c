// Host-side helpers for tools/preview.py (loaded with ctypes): the watch's
// own mandel.c, dive.c, grid.c and face.c, over a whole screen at once.

#include <string.h>

#include "../../src/c/dive.h"
#include "../../src/c/face.h"
#include "../../src/c/generated/palette.h"

// The watch whose dives the previews show: its seed and the date. Hours
// map to dives through dive_hour_seed, as on the watch.
static uint32_t s_watch_seed = 0x5EED2026;
static int s_year = 2026, s_yday = 326;

void render_set_watch(uint32_t watch_seed, int year, int yday) {
  s_watch_seed = watch_seed;
  s_year = year;
  s_yday = yday;
}

uint32_t render_hour_seed(int hour) {
  return dive_hour_seed(s_watch_seed, s_year, s_yday, hour);
}

// The view of an hour's dive at a minute, as the watch computes it.
static void view_at(int hour, int minute, int w, int h, MandelView *view) {
  dive_view(view, render_hour_seed(hour), minute, w, h);
}

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
  view_at(hour, minute, w, h, view);
  render_view(view, w, h, out);
}

// ---------------------------------------------------------------------------
// Color watches' face

// The packed cells of a time on a w x h screen in cells of 1 << shift px, as
// the watch computes them. `out` holds the grid's (w >> shift) x (h >> shift)
// cells, rounded up.
int render_face_grid(int hour, int minute, int w, int h, int shift, uint8_t *out) {
  const GridEncoding encoding = {
    .lace_step = LACE_STEP,
    .low_percent = DEPTH_LOW_PERCENT,
    .high_percent = DEPTH_HIGH_PERCENT,
  };
  Grid *grid = grid_create(w, h, shift, PALETTE_SIZE, encoding);
  if (!grid) return 0;
  MandelView view;
  view_at(hour, minute, grid->w, grid->h, &view);
  grid_start(grid, &view);
  while (grid_step(grid)) {
  }
  memcpy(out, grid->cells, (size_t)grid->w * grid->h);
  grid_destroy(grid);
  return 1;
}

// One frame of the face as color codes: palette entries 0..PALETTE_SIZE-1,
// then digit, keyline, inside and lace (FACE_CODE_*).
enum {
  FACE_CODE_DIGIT = PALETTE_SIZE,
  FACE_CODE_KEYLINE,
  FACE_CODE_INSIDE,
  FACE_CODE_LACE,
};

int face_code_count(void) {
  return FACE_CODE_LACE + 1;
}

void render_face(const uint8_t *cells, int w, int h, int shift, const DistField *dist,
                 const uint8_t style_bytes[6], int phase, uint8_t *out) {
  uint8_t codes[PALETTE_SIZE];
  for (int i = 0; i < PALETTE_SIZE; i++) codes[i] = (uint8_t)i;
  const FaceColors colors = {
    .palette = codes,
    .size = PALETTE_SIZE,
    .digit = FACE_CODE_DIGIT,
    .keyline = FACE_CODE_KEYLINE,
    .inside = FACE_CODE_INSIDE,
    .lace = FACE_CODE_LACE,
  };
  const FaceStyle style = {style_bytes[0], style_bytes[1], style_bytes[2],
                           style_bytes[3], style_bytes[4], style_bytes[5]};
  static FaceFrame frame;
  face_frame(&frame, &style, &colors, (uint8_t)phase);
  const int cell = 1 << shift;
  const int grid_w = (w + cell - 1) >> shift;
  for (int y = 0; y < h; y++) {
    face_row(&frame, cells + (size_t)(y >> shift) * grid_w, (uint8_t)shift, dist, y, 0, w - 1,
             out + (size_t)y * w);
  }
}
