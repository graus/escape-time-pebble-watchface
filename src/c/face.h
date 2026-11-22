#pragma once
// The color watches' face: white digits that count as part of the set, a
// black keyline, bands flowing out of the digits, and the dive everywhere
// else. Plain C with no Pebble dependency, so the host previews shade with
// the code the watch runs.
//
// The digits are not bitmaps: each is a distance field around its centre
// lines (tools/glyphs.py), and a pixel is digit when its distance is under
// its stroke radius. The radius is looked up per frame from the pixel's grid
// cell (see FaceStyle), so the strokes are fine where the view is shallow,
// bold where it is deep, and swell in waves that ride the palette's phase.
//
// Distances are in quarter pixels (FACE_DIST_ONE), saturating at
// FACE_DIST_FAR.

#include <stdint.h>

#include "grid.h"

#define FACE_DIST_ONE 4
#define FACE_DIST_FAR 0xFF
#define FACE_HALO_MAX 64  // quarter pixels

// The distances of the whole screen to its digits' centre lines, sampled
// every 2 px: sample (i, j) sits on pixel (2i, 2j). There is one sample
// beyond the last pixel each way, so odd pixels can always average two.
typedef struct {
  uint16_t w, h;
  uint8_t *d;
} DistField;

// A field for a w x h pixel screen, all FACE_DIST_FAR. NULL on out-of-memory.
DistField *dist_create(uint16_t screen_w, uint16_t screen_h);
void dist_destroy(DistField *dist);
void dist_clear(DistField *dist);

// Merges one row of a digit's field: `n` samples whose first sits on sample
// (i, j) of the screen field, keeping the nearer distance where digits'
// fields overlap. Samples off the screen are skipped.
void dist_merge_row(DistField *dist, int i, int j, const uint8_t *row, int n);

// Stroke weights, in quarter pixels, for one size of digit. A pixel's stroke
// radius is
//
//   base + depth_weight * (d / D)^2 + tide_weight * (t / T)^2
//
// with d its cell's depth (0..D = GRID_DEPTH_MAX) and t the tide: with
// P half the palette and m = (band + phase) mod P, t = P - |2m - P|, a
// triangle wave over the bands that the phase carries outward with them. Its
// crest T is P, or P - 1 when P is odd (m is whole). Points in the set count
// as d = D, t = T; lace keeps its depth with t = 0.
//
// The base must allow for the distance field's 2 px sampling: interpolated
// distances overshoot by up to 1 px near a centre line.
typedef struct {
  uint8_t base, depth_weight, tide_weight;
  uint8_t keyline_width;  // black ring outside the stroke
  uint8_t halo_width;     // bands around the keyline
  uint8_t halo_band;      // one band of the halo (at least 1)
} FaceStyle;

// The colors the face uses: GColor8 bytes on the watch, any codes on the host.
typedef struct {
  const uint8_t *palette;
  uint8_t size;  // palette length: even, at most GRID_BAND_LACE
  uint8_t digit, keyline, inside, lace;
} FaceColors;

// Everything a frame needs per cell value and per distance, so shading a
// pixel is a few lookups.
typedef struct {
  uint8_t color[256];   // cell → its color in the dive
  uint8_t radius[256];  // cell → stroke radius
  uint8_t keyline_width, halo_width;
  uint8_t halo_color[FACE_HALO_MAX];  // distance past the keyline → color
  uint8_t digit_color, keyline_color;
} FaceFrame;

// Prepares the lookups for one phase of the palette (0..size-1).
void face_frame(FaceFrame *frame, const FaceStyle *style, const FaceColors *colors,
                uint8_t phase);

// Shades pixels x0..x1 of screen row y into out[x0..x1]. `cells` is the
// grid row under y (cells of 1 << shift pixels).
void face_row(const FaceFrame *frame, const uint8_t *cells, uint8_t shift,
              const DistField *dist, int y, int x0, int x1, uint8_t *out);
