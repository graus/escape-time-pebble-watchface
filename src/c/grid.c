#include "grid.h"

#include <stdlib.h>
#include <string.h>

#include "dive.h"

// At most this many cells are sampled to find a view's depth quantiles.
#define DEPTH_SAMPLES 256

Grid *grid_create(uint16_t w, uint16_t h, uint8_t shift, uint8_t modulo,
                  GridEncoding encoding) {
  if (encoding.packed && modulo > GRID_BAND_LACE) {
    return NULL;  // the bands would collide with lace and inside
  }
  Grid *grid = malloc(sizeof(Grid));
  if (!grid) {
    return NULL;
  }
  uint16_t cell = 1 << shift;
  grid->w = (w + cell - 1) >> shift;
  grid->h = (h + cell - 1) >> shift;
  grid->shift = shift;
  grid->modulo = modulo;
  grid->encoding = encoding;
  grid->cells = malloc((size_t)grid->w * grid->h);
  if (!grid->cells) {
    free(grid);
    return NULL;
  }
  memset(grid->cells, encoding.packed ? GRID_PACKED_INSIDE : GRID_INSIDE,
         (size_t)grid->w * grid->h);
  memset(&grid->view, 0, sizeof(grid->view));
  grid->depth_low = grid->depth_high = 0;
  grid->next = grid->h;  // nothing to compute until a view is set
  return grid;
}

void grid_destroy(Grid *grid) {
  if (grid) {
    free(grid->cells);
    free(grid);
  }
}

static uint16_t escape(const Grid *grid, uint16_t x, uint16_t y) {
  int32_t cr, ci;
  dive_point(&grid->view, x, y, grid->w, grid->h, &cr, &ci);
  return mandel_escape(cr, ci, grid->view.max_iter);
}

// The escape counts of a sparse lattice of cells, sorted, give the view's
// depth quantiles before the full view is known.
static void find_depth_range(Grid *grid) {
  uint16_t counts[DEPTH_SAMPLES];
  int n = 0;
  // The smallest lattice step that keeps the samples within the buffer.
  uint16_t step = 1;
  while ((uint32_t)((grid->w + step - 1) / step) * ((grid->h + step - 1) / step) > DEPTH_SAMPLES) {
    step++;
  }
  for (uint16_t y = step / 2; y < grid->h; y += step) {
    for (uint16_t x = step / 2; x < grid->w; x += step) {
      uint16_t count = escape(grid, x, y);
      if (count == MANDEL_INSIDE) {
        continue;  // depth ranks escaping points only
      }
      // Insertion sort: a few hundred values, once a minute.
      int i = n++;
      while (i > 0 && counts[i - 1] > count) {
        counts[i] = counts[i - 1];
        i--;
      }
      counts[i] = count;
    }
  }
  if (n == 0) {
    grid->depth_low = grid->depth_high = 0;
    return;
  }
  grid->depth_low = counts[(n - 1) * grid->encoding.low_percent / 100];
  grid->depth_high = counts[(n - 1) * grid->encoding.high_percent / 100];
}

void grid_start(Grid *grid, const MandelView *view) {
  grid->view = *view;
  if (grid->encoding.packed) {
    find_depth_range(grid);
  }
  grid->next = 0;
}

// The n-th row to compute: rows 0, 8, 16, ... first, then 4, 12, ..., then
// 2, 6, ... and finally the odd rows, so each pass halves the gaps.
static uint16_t interlaced_row(uint16_t n, uint16_t h) {
  static const uint8_t offsets[8] = {0, 4, 2, 6, 1, 5, 3, 7};
  for (int pass = 0; pass < 8; pass++) {
    uint16_t rows = h > offsets[pass] ? (h - offsets[pass] + 7) / 8 : 0;
    if (n < rows) {
      return offsets[pass] + n * 8;
    }
    n -= rows;
  }
  return 0;
}

// Depth 0..GRID_DEPTH_MAX of an escape count, rounded, linear between the
// view's quantiles.
static uint8_t depth(const Grid *grid, uint16_t count) {
  uint16_t low = grid->depth_low, high = grid->depth_high;
  if (count <= low) return 0;
  if (count >= high) return GRID_DEPTH_MAX;
  uint32_t span = high - low;
  return (uint8_t)(((uint32_t)(count - low) * GRID_DEPTH_MAX + span / 2) / span);
}

// Lace: the count jumps from the left neighbour's, or the left neighbour is
// in the set (this cell is on its edge).
static bool is_lace(const Grid *grid, uint16_t count, uint16_t left) {
  if (left == MANDEL_INSIDE) {
    return true;
  }
  uint16_t gap = count > left ? count - left : left - count;
  return gap > grid->encoding.lace_step;
}

static uint8_t pack(const Grid *grid, uint16_t count, uint16_t left, bool has_left) {
  if (count == MANDEL_INSIDE) {
    return GRID_PACKED_INSIDE;
  }
  uint8_t band = has_left && is_lace(grid, count, left) ? GRID_BAND_LACE
                                                        : count % grid->modulo;
  return (uint8_t)(depth(grid, count) << GRID_BAND_BITS | band);
}

bool grid_step(Grid *grid) {
  if (grid_done(grid)) {
    return false;
  }
  uint16_t y = interlaced_row(grid->next, grid->h);
  uint8_t *row = grid->cells + (size_t)y * grid->w;
  uint16_t left = 0;
  for (uint16_t x = 0; x < grid->w; x++) {
    uint16_t count = escape(grid, x, y);
    if (grid->encoding.packed) {
      row[x] = pack(grid, count, left, x > 0);
    } else {
      row[x] = count == MANDEL_INSIDE ? GRID_INSIDE : (uint8_t)(count % grid->modulo);
    }
    left = count;
  }
  grid->next++;
  return !grid_done(grid);
}
