#include "face.h"

#include <stdlib.h>
#include <string.h>

DistField *dist_create(uint16_t screen_w, uint16_t screen_h) {
  DistField *dist = malloc(sizeof(DistField));
  if (!dist) {
    return NULL;
  }
  // Pixel 2i + 1 averages samples i and i + 1, so the last pixel, w - 1,
  // needs sample (w - 1) / 2 + 1.
  dist->w = (screen_w - 1) / 2 + 2;
  dist->h = (screen_h - 1) / 2 + 2;
  dist->d = malloc((size_t)dist->w * dist->h);
  if (!dist->d) {
    free(dist);
    return NULL;
  }
  dist_clear(dist);
  return dist;
}

void dist_destroy(DistField *dist) {
  if (dist) {
    free(dist->d);
    free(dist);
  }
}

void dist_clear(DistField *dist) {
  memset(dist->d, FACE_DIST_FAR, (size_t)dist->w * dist->h);
}

void dist_merge_row(DistField *dist, int i, int j, const uint8_t *row, int n) {
  if (j < 0 || j >= dist->h) {
    return;
  }
  // The part of the row on the field: row[k] lands on sample i + k.
  int first = i < 0 ? -i : 0;
  int last = i + n > dist->w ? dist->w - i : n;  // exclusive
  uint8_t *line = dist->d + (size_t)j * dist->w;
  for (int k = first; k < last; k++) {
    if (row[k] < line[i + k]) {
      line[i + k] = row[k];
    }
  }
}

// weight * (v / max)^2, rounded.
static int scaled_square(int weight, int v, int max) {
  return (weight * v * v + max * max / 2) / (max * max);
}

void face_frame(FaceFrame *frame, const FaceStyle *style, const FaceColors *colors,
                uint8_t phase) {
  const int size = colors->size;
  const int tide_period = size / 2;
  const int tide_crest = tide_period - tide_period % 2;
  for (int cell = 0; cell < 256; cell++) {
    int band = grid_band(cell);
    int depth = grid_depth(cell);
    int tide;
    if (band < size) {
      frame->color[cell] = colors->palette[(band + phase) % size];
      // Cell counts are bands, and the phase moves each color one band
      // outward per frame; the wave is a function of (band + phase) too, so
      // its crests travel outward with the colors.
      int m = (band + phase) % tide_period;
      tide = tide_period - abs(2 * m - tide_period);
    } else if (band == GRID_BAND_LACE) {
      // A filament has no steady band for the wave to ride: it keeps its
      // depth but does not swell, or strokes would sprout hairs.
      frame->color[cell] = colors->lace;
      tide = 0;
    } else {
      // The set itself: as deep and as swollen as strokes get.
      frame->color[cell] = colors->inside;
      depth = GRID_DEPTH_MAX;
      tide = tide_crest;
    }
    int radius = style->base +
                 scaled_square(style->depth_weight, depth, GRID_DEPTH_MAX) +
                 scaled_square(style->tide_weight, tide, tide_crest);
    frame->radius[cell] = radius < FACE_DIST_FAR ? radius : FACE_DIST_FAR;
  }

  frame->keyline_width = style->keyline_width;
  frame->halo_width = style->halo_width < FACE_HALO_MAX ? style->halo_width : FACE_HALO_MAX;
  const int halo_band = style->halo_band ? style->halo_band : 1;
  for (int q = 0; q < frame->halo_width; q++) {
    // Band k of the halo shows palette entry (phase - k): as the phase grows
    // each color moves to the next band out, so the halo flows away from
    // the digit as the dive's bands flow away from the set.
    int k = (q / halo_band) % size;
    frame->halo_color[q] = colors->palette[(phase + size - k) % size];
  }
  frame->digit_color = colors->digit;
  frame->keyline_color = colors->keyline;
}

void face_row(const FaceFrame *frame, const uint8_t *cells, uint8_t shift,
              const DistField *dist, int y, int x0, int x1, uint8_t *out) {
  // Even rows lie on a sample row; odd rows average the two around them.
  const uint8_t *a = dist->d + (size_t)(y >> 1) * dist->w;
  const uint8_t *b = (y & 1) ? a + dist->w : a;
  const int keyline_end = frame->keyline_width;
  const int halo_end = keyline_end + frame->halo_width;
  for (int x = x0; x <= x1; x++) {
    int i = x >> 1;
    // Four times the distance: the pixel's sample counted four times, or
    // its two (or four) neighbouring samples, bilinearly.
    int sum4 = a[i] + b[i];
    sum4 = (x & 1) ? sum4 + a[i + 1] + b[i + 1] : 2 * sum4;
    uint8_t cell = cells[x >> shift];
    int past = ((sum4 + 2) >> 2) - frame->radius[cell];
    if (past < 0) {
      out[x] = frame->digit_color;
    } else if (past < keyline_end) {
      out[x] = frame->keyline_color;
    } else if (past < halo_end) {
      out[x] = frame->halo_color[past - keyline_end];
    } else {
      out[x] = frame->color[cell];
    }
  }
}
