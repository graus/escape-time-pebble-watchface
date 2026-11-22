#include "paint.h"

#include "generated/palette.h"

#if defined(PBL_COLOR)

void paint_face(GBitmap *fb, const FaceFrame *frame, const Grid *grid,
                const DistField *dist) {
  GRect bounds = gbitmap_get_bounds(fb);
  int rows = bounds.size.h;
  if (rows > (grid->h << grid->shift)) rows = grid->h << grid->shift;
  for (int y = 0; y < rows; y++) {
    // Row info gives the real x range of each row, which matters on round
    // screens where rows are shorter than the display width.
    GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);
    face_row(frame, grid->cells + (y >> grid->shift) * grid->w, grid->shift, dist,
             y, info.min_x, info.max_x, info.data);
  }
}

#else  // PBL_BW

// 4x4 Bayer matrix: thresholds 0..15 spread so any level looks even.
static const uint8_t s_bayer[4][4] = {
  {0, 8, 2, 10},
  {12, 4, 14, 6},
  {3, 11, 1, 9},
  {15, 7, 13, 5},
};

// The 8-pixel pattern byte of a level (0..16 lit pixels out of 16) on a
// pattern row; the 1-bit framebuffer stores the leftmost pixel in bit 0.
static uint8_t pattern(int level, int py) {
  uint8_t nibble = 0;
  for (int px = 0; px < 4; px++) {
    if (s_bayer[py][px] < level) nibble |= 1 << px;
  }
  return nibble | nibble << 4;
}

void paint(GBitmap *fb, const Grid *grid, GRect window, uint8_t phase) {
  if (gbitmap_get_format(fb) != GBitmapFormat1Bit || grid->shift != 1) {
    return;  // B&W cells are always 2 px: four to a framebuffer byte
  }
  // This frame's pattern byte per cell value and pattern row.
  static uint8_t lut[4][256];
  for (int py = 0; py < 4; py++) {
    for (int v = 0; v < PALETTE_SIZE; v++) {
      lut[py][v] = pattern(s_palette_bw[(v + phase) % PALETTE_SIZE], py);
    }
    lut[py][GRID_INSIDE] = 0xFF;
  }

  GRect fb_bounds = gbitmap_get_bounds(fb);
  uint16_t bpr = gbitmap_get_bytes_per_row(fb);
  uint8_t *data = gbitmap_get_data(fb);
  int top = window.origin.y;
  int first_byte = window.origin.x >> 3;
  int bytes = window.size.w >> 3;
  if (first_byte < 0 || first_byte + bytes > bpr || bytes * 4 > grid->w) {
    return;
  }
  int y0 = top > 0 ? top : 0;
  int y1 = top + window.size.h;
  if (y1 > fb_bounds.size.h) y1 = fb_bounds.size.h;
  for (int y = y0; y < y1; y++) {
    int cy = (y - top) >> 1;
    if (cy >= grid->h) break;
    const uint8_t *cells = grid->cells + cy * grid->w;
    const uint8_t *pat = lut[y & 3];
    uint8_t *row = data + y * bpr + first_byte;
    for (int b = 0; b < bytes; b++) {
      if (!row[b]) continue;  // all black: nothing to paint
      const uint8_t *c = cells + 4 * b;
      // Each 2 px cell takes its two bits of its own pattern.
      uint8_t mask = (pat[c[0]] & 0x03) | (pat[c[1]] & 0x0C) |
                     (pat[c[2]] & 0x30) | (pat[c[3]] & 0xC0);
      row[b] &= mask;
    }
  }
}

#endif
