#pragma once
// The window's escape counts, one byte per cell, computed a row at a time so
// a minute's new view spreads over many frames. Plain C with no Pebble
// dependency.
//
// A cell holds its iteration count mod PALETTE_SIZE (the renderer adds the
// moving phase), or GRID_INSIDE for points in the set. Rows are computed
// interlaced (every 8th row first, then the ones between), so the new view
// sharpens evenly over the old one instead of wiping down the screen.

#include <stdbool.h>
#include <stdint.h>

#include "mandel.h"

#define GRID_INSIDE 0xFF

typedef struct {
  uint16_t w, h;       // in cells
  uint8_t shift;       // cell size: 1 << shift pixels
  uint8_t modulo;      // palette length the counts are reduced by
  uint8_t *cells;      // w * h
  MandelView view;
  uint16_t next;       // rows computed so far in this view (h when done)
} Grid;

// A grid for a window of w x h pixels in cells of 1 << shift pixels, filled
// with GRID_INSIDE. Returns NULL on out-of-memory.
Grid *grid_create(uint16_t win_w, uint16_t win_h, uint8_t shift, uint8_t modulo);
void grid_destroy(Grid *grid);

// Starts computing a new view; the old counts stay until overwritten.
void grid_start(Grid *grid, const MandelView *view);

// Computes the next row. Returns false when the view is complete.
bool grid_step(Grid *grid);

static inline bool grid_done(const Grid *grid) {
  return grid->next >= grid->h;
}
