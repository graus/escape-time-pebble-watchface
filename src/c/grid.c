#include "grid.h"

#include <stdlib.h>
#include <string.h>

#include "dive.h"

Grid *grid_create(uint16_t win_w, uint16_t win_h, uint8_t shift, uint8_t modulo) {
  Grid *grid = malloc(sizeof(Grid));
  if (!grid) {
    return NULL;
  }
  uint16_t cell = 1 << shift;
  grid->w = (win_w + cell - 1) >> shift;
  grid->h = (win_h + cell - 1) >> shift;
  grid->shift = shift;
  grid->modulo = modulo;
  grid->cells = malloc((size_t)grid->w * grid->h);
  if (!grid->cells) {
    free(grid);
    return NULL;
  }
  memset(grid->cells, GRID_INSIDE, (size_t)grid->w * grid->h);
  memset(&grid->view, 0, sizeof(grid->view));
  grid->next = grid->h;  // nothing to compute until a view is set
  return grid;
}

void grid_destroy(Grid *grid) {
  if (grid) {
    free(grid->cells);
    free(grid);
  }
}

void grid_start(Grid *grid, const MandelView *view) {
  grid->view = *view;
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

bool grid_step(Grid *grid) {
  if (grid_done(grid)) {
    return false;
  }
  uint16_t y = interlaced_row(grid->next, grid->h);
  uint8_t *row = grid->cells + (size_t)y * grid->w;
  for (uint16_t x = 0; x < grid->w; x++) {
    int32_t cr, ci;
    dive_point(&grid->view, x, y, grid->w, grid->h, &cr, &ci);
    uint16_t n = mandel_escape(cr, ci, grid->view.max_iter);
    row[x] = n == MANDEL_INSIDE ? GRID_INSIDE : (uint8_t)(n % grid->modulo);
  }
  grid->next++;
  return !grid_done(grid);
}
