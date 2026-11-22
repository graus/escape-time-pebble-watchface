#pragma once
// The view's escape counts, one byte per cell, computed a row at a time so
// a minute's new view spreads over many frames. Plain C with no Pebble
// dependency.
//
// Rows are computed interlaced (every 8th row first, then the ones between),
// so the new view sharpens evenly over the old one instead of wiping down the
// screen.
//
// A cell holds one of two encodings:
//
//   plain  (B&W watches): the count mod `modulo` (the renderer adds the
//          moving phase), or GRID_INSIDE for points in the set.
//   packed (color watches): in the high 3 bits the depth, where the count
//          ranks between the view's low and high quantiles (0..
//          GRID_DEPTH_MAX); in the low 5 bits the count mod `modulo`, or
//          GRID_BAND_LACE for a cell whose count is more than `lace_step`
//          from its left neighbour's (a filament finer than a cell, drawn in
//          one steady color instead of confetti), or GRID_BAND_INSIDE.

#include <stdbool.h>
#include <stdint.h>

#include "mandel.h"

#define GRID_INSIDE 0xFF

#define GRID_BAND_BITS 5
#define GRID_BAND_MASK ((1 << GRID_BAND_BITS) - 1)
#define GRID_BAND_LACE 30
#define GRID_BAND_INSIDE 31
#define GRID_DEPTH_MAX 7
// Points in the set are as deep as cells get.
#define GRID_PACKED_INSIDE (GRID_DEPTH_MAX << GRID_BAND_BITS | GRID_BAND_INSIDE)

static inline uint8_t grid_band(uint8_t cell) {
  return cell & GRID_BAND_MASK;
}

static inline uint8_t grid_depth(uint8_t cell) {
  return cell >> GRID_BAND_BITS;
}

typedef struct {
  bool packed;          // packed (else plain) encoding
  uint8_t lace_step;    // packed only
  uint8_t low_percent;  // packed only: depth 0 at or below this quantile
  uint8_t high_percent; //              and GRID_DEPTH_MAX at or above this one
} GridEncoding;

typedef struct {
  uint16_t w, h;        // in cells
  uint8_t shift;        // cell size: 1 << shift pixels
  uint8_t modulo;       // palette length the counts are reduced by
  GridEncoding encoding;
  uint8_t *cells;       // w * h
  MandelView view;
  uint16_t depth_low, depth_high;  // this view's quantiles (packed only)
  uint16_t next;        // rows computed so far in this view (h when done)
} Grid;

// A grid for an area of w x h pixels in cells of 1 << shift pixels, filled
// with points in the set. `modulo` must be at most GRID_BAND_LACE when packed.
// Returns NULL on out-of-memory.
Grid *grid_create(uint16_t w, uint16_t h, uint8_t shift, uint8_t modulo,
                  GridEncoding encoding);
void grid_destroy(Grid *grid);

// Starts computing a new view; the old cells stay until overwritten. Packed
// grids first sample the view sparsely to find its depth quantiles.
void grid_start(Grid *grid, const MandelView *view);

// Computes the next row. Returns false when the view is complete.
bool grid_step(Grid *grid);

static inline bool grid_done(const Grid *grid) {
  return grid->next >= grid->h;
}
