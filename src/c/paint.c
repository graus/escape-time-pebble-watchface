#include "paint.h"

// Rows of the screen the grid covers (it may round up past the screen).
static int painted_rows(GBitmap *fb, const Grid *grid) {
  int rows = gbitmap_get_bounds(fb).size.h;
  int covered = grid->h << grid->shift;
  return rows < covered ? rows : covered;
}

static const uint8_t *grid_row(const Grid *grid, int y) {
  return grid->cells + (y >> grid->shift) * grid->w;
}

#if defined(PBL_COLOR)

void paint_face(GBitmap *fb, const FaceFrame *frame, const Grid *grid,
                const DistField *dist) {
  int rows = painted_rows(fb, grid);
  for (int y = 0; y < rows; y++) {
    // Row info gives the real x range of each row, which matters on round
    // screens where rows are shorter than the display width.
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    face_row(frame, grid_row(grid, y), grid->shift, dist, y, info.min_x, info.max_x,
             info.data);
  }
}

#else  // PBL_BW

#define LEVELS 17  // lit pixels out of 16: 0..16

// 4x4 Bayer matrix: thresholds 0..15 spread so any level looks even.
static const uint8_t s_bayer[4][4] = {
  {0, 8, 2, 10},
  {12, 4, 14, 6},
  {3, 11, 1, 9},
  {15, 7, 13, 5},
};

// Per pattern row and level, the pattern for 8 pixels; the 1-bit
// framebuffer stores the leftmost pixel in bit 0.
static uint8_t s_patterns[4][LEVELS];

static void build_patterns(void) {
  for (int py = 0; py < 4; py++) {
    for (int level = 0; level < LEVELS; level++) {
      uint8_t nibble = 0;
      for (int px = 0; px < 4; px++) {
        if (s_bayer[py][px] < level) nibble |= 1 << px;
      }
      s_patterns[py][level] = nibble | nibble << 4;
    }
  }
}

void paint_face(GBitmap *fb, const FaceFrame *frame, const Grid *grid,
                const DistField *dist) {
  if (gbitmap_get_format(fb) != GBitmapFormat1Bit) {
    return;
  }
  if (!s_patterns[0][LEVELS - 1]) {
    build_patterns();  // once: level 16 lights every pixel
  }
  const int w = gbitmap_get_bounds(fb).size.w;
  const int rows = painted_rows(fb, grid);
  const uint16_t bytes_per_row = gbitmap_get_bytes_per_row(fb);
  uint8_t *data = gbitmap_get_data(fb);
  uint8_t levels[PBL_DISPLAY_WIDTH];
  for (int y = 0; y < rows; y++) {
    face_row(frame, grid_row(grid, y), grid->shift, dist, y, 0, w - 1, levels);
    const uint8_t *pattern = s_patterns[y & 3];
    uint8_t *row = data + y * bytes_per_row;
    // Each pixel takes its own bit of its level's pattern.
    for (int x0 = 0; x0 < w; x0 += 8) {
      int n = w - x0 < 8 ? w - x0 : 8;
      uint8_t byte = 0;
      for (int k = 0; k < n; k++) {
        byte |= pattern[levels[x0 + k]] & (1 << k);
      }
      row[x0 >> 3] = byte;
    }
  }
}

#endif
