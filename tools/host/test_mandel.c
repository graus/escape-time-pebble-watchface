// Host tests for the fractal engine and the color face: the watch's
// mandel.c, dive.c, grid.c and face.c, compiled natively.
//
//   make -C tools/host test

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/c/dive.h"
#include "../../src/c/generated/targets.h"
#include "../../src/c/face.h"
#include "../../src/c/grid.h"

static int failures;

#define CHECK(cond, ...)                \
  do {                                  \
    if (!(cond)) {                      \
      printf("FAIL %s:%d: ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);              \
      printf("\n");                     \
      failures++;                       \
    }                                   \
  } while (0)

static int escape_double(double cr, double ci, int max_iter) {
  double x = 0, y = 0;
  for (int i = 0; i < max_iter; i++) {
    if (x * x + y * y > 4) return i;
    double t = x * x - y * y + cr;
    y = 2 * x * y + ci;
    x = t;
  }
  return MANDEL_INSIDE;
}

// Fixed point agrees with double precision on a coarse grid over the set.
static void test_matches_double(void) {
  int total = 0, same = 0;
  for (int py = 0; py < 120; py++) {
    for (int px = 0; px < 160; px++) {
      double cr = -2.2 + px * 2.9 / 160, ci = -1.2 + py * 2.4 / 120;
      int a = mandel_escape(MANDEL_FIX(cr), MANDEL_FIX(ci), 200);
      int b = escape_double(cr, ci, 200);
      total++;
      if (a == b) same++;
    }
  }
  CHECK(same * 1000 >= total * 995, "fixed point matches double on %d of %d points", same, total);
}

static void test_known_points(void) {
  CHECK(mandel_escape(0, 0, 100) == MANDEL_INSIDE, "0 is inside");
  CHECK(mandel_escape(MANDEL_FIX(-1.0), 0, 100) == MANDEL_INSIDE, "-1 is inside");
  CHECK(mandel_escape(MANDEL_FIX(-0.1), MANDEL_FIX(0.7), 500) == MANDEL_INSIDE,
        "-0.1+0.7i is inside (period-3 bulb, caught by the cycle check)");
  // 0, 1, 2, 5: |2|^2 = 4 is not beyond the radius, 5 is.
  CHECK(mandel_escape(MANDEL_FIX(1.0), 0, 100) == 3, "1 escapes after 3");
  CHECK(mandel_escape(MANDEL_FIX(0.5), 0, 100) == 5, "0.5 escapes after 5");
}

// :00 shows the whole set, upright, around the home center; :59 is
// (6/5)^59 deeper. The same seed is the same dive; another hour, day or
// watch is another dive; the spin turns the view without stretching it.
static bool same_view(const MandelView *a, const MandelView *b) {
  // Field by field: MandelView has padding, which memcmp would compare too.
  return a->cr == b->cr && a->ci == b->ci && a->step == b->step &&
         a->ux == b->ux && a->uy == b->uy && a->max_iter == b->max_iter;
}

// The target an hour dives into: the one (or its mirror) that the :59 view's
// center has closed in on. Returns false if there is none.
static bool find_target(const MandelView *v59, double *tr, double *ti) {
  double cr = v59->cr / (double)MANDEL_ONE, ci = v59->ci / (double)MANDEL_ONE;
  for (int k = 0; k < DIVE_TARGETS; k++) {
    for (int sign = -1; sign <= 1; sign += 2) {
      double r = s_targets[k][0] / (double)MANDEL_ONE, i = sign * s_targets[k][1] / (double)MANDEL_ONE;
      // At :59 the center is |home - target| * (5/6)^59 from the target.
      double expected = hypot(-0.75 - r, -i) * pow(5.0 / 6, 59);
      if (fabs(hypot(cr - r, ci - i) - expected) < 3.0 * v59->step / MANDEL_ONE) {
        *tr = r;
        *ti = i;
        return true;
      }
    }
  }
  return false;
}

// The complex point under a (fractional) screen position, as dive_point.
static void point_at(const MandelView *v, double dx, double dy, double *r, double *i) {
  *r = (v->cr + dx * v->ux + dy * v->uy) / (double)MANDEL_ONE;
  *i = (v->ci + dx * v->uy - dy * v->ux) / (double)MANDEL_ONE;
}

static void test_dive(void) {
  const uint32_t seed = dive_hour_seed(12345, 2026, 326, 16);
  MandelView v0, v59, again;
  dive_view(&v0, seed, 0, 135, 146);
  dive_view(&v59, seed, 59, 135, 146);
  dive_view(&again, seed, 59, 135, 146);
  CHECK(abs(v0.cr - MANDEL_FIX(-0.75)) < 2 && v0.ci == 0, "home center at :00");
  CHECK(v0.ux == v0.step && v0.uy == 0, ":00 is upright");
  double zoom = (double)v0.step / v59.step;
  CHECK(zoom > 40000 && zoom < 55000, "zoom at :59 is (6/5)^59 = 47,000 (got %.0f)", zoom);
  CHECK(v59.step >= 32, "a pixel at :59 still spans %d units of 2^-27", v59.step);
  CHECK(same_view(&v59, &again), "the same seed is the same dive");

  CHECK(dive_hour_seed(12345, 2026, 326, 16) != dive_hour_seed(12345, 2026, 326, 17) &&
        dive_hour_seed(12345, 2026, 326, 16) != dive_hour_seed(12345, 2026, 327, 16) &&
        dive_hour_seed(12345, 2026, 326, 16) != dive_hour_seed(12346, 2026, 326, 16),
        "another hour, day or watch has another seed");

  // 240 hours (ten days) of one watch: every :30 view is its own, and the
  // seeds reach most of the targets, both ways up.
  enum { HOURS = 240 };
  static MandelView views[HOURS];
  int same = 0, upper = 0;
  for (int k = 0; k < HOURS; k++) {
    dive_view(&views[k], dive_hour_seed(777, 2026, 300 + k / 24, k % 24), 30, 135, 146);
    MandelView deep;
    dive_view(&deep, dive_hour_seed(777, 2026, 300 + k / 24, k % 24), 59, 135, 146);
    upper += deep.ci > 0;
    for (int j = 0; j < k; j++) {
      same += same_view(&views[j], &views[k]);
    }
  }
  CHECK(same == 0, "%d of %d hours repeat another's :30 view", same, HOURS);
  CHECK(upper > HOURS / 4 && upper < HOURS * 3 / 4, "%d of %d dives above the axis", upper, HOURS);

  // The spin turns the step without changing its length (to the rounding of
  // the sine table), and has turned by :30.
  int turned = 0;
  for (int k = 0; k < HOURS; k++) {
    double len = sqrt((double)views[k].ux * views[k].ux + (double)views[k].uy * views[k].uy);
    CHECK(fabs(len / views[k].step - 1) < 0.02, "hour %d: step %d, turned %.0f", k, views[k].step, len);
    turned += views[k].uy != 0;
  }
  CHECK(turned > HOURS * 9 / 10, "%d of %d views turned by :30", turned, HOURS);

  // Each dive closes in on one of the targets (or its mirror), and the
  // target keeps its place on the screen as the view zooms and turns: the
  // screen position it has at :00 shows it at :30 and :59 too.
  int lost = 0, moved = 0;
  for (int k = 0; k < 48; k++) {
    uint32_t s = dive_hour_seed(4242, 2026, 326 + k / 24, k % 24);
    MandelView at0, at30, at59;
    dive_view(&at0, s, 0, 144, 168);
    dive_view(&at30, s, 30, 144, 168);
    dive_view(&at59, s, 59, 144, 168);
    double tr, ti;
    if (!find_target(&at59, &tr, &ti)) {
      lost++;
      continue;
    }
    // :00 is upright: the target's screen offset from the center.
    double dx = (tr - at0.cr / (double)MANDEL_ONE) * MANDEL_ONE / at0.step;
    double dy = -(ti - at0.ci / (double)MANDEL_ONE) * MANDEL_ONE / at0.step;
    const MandelView *later[2] = {&at30, &at59};
    for (int n = 0; n < 2; n++) {
      double r, i;
      point_at(later[n], dx, dy, &r, &i);
      moved += hypot(r - tr, i - ti) > 2.0 * later[n]->step / MANDEL_ONE;
    }
  }
  CHECK(lost == 0, "%d of 48 dives closed in on no target", lost);
  CHECK(moved == 0, "%d of 96 views moved the target on the screen", moved);
}

// Every row is computed exactly once, whatever the height.
static void test_interlace(void) {
  for (int h = 1; h <= 40; h++) {
    Grid *g = grid_create(8, h, 0, 24, (GridEncoding){6, 25, 97});
    // Far outside: escapes at once, cell 0.
    MandelView v = {.cr = MANDEL_FIX(3.0), .ci = 0, .step = 1, .ux = 1, .uy = 0, .max_iter = 1};
    memset(g->cells, 0xAA, (size_t)g->w * g->h);
    grid_start(g, &v);
    int steps = 0;
    while (!grid_done(g)) {
      grid_step(g);
      steps++;
    }
    int untouched = 0;
    for (int i = 0; i < g->w * g->h; i++) {
      if (g->cells[i] == 0xAA) untouched++;
    }
    CHECK(steps == h && untouched == 0, "height %d: %d steps, %d cells untouched", h, steps, untouched);
    grid_destroy(g);
  }
}

static const GridEncoding PACKED = {
  .lace_step = 6, .low_percent = 25, .high_percent = 97,
};

static Grid *computed(int hour, int minute, int w, int h, uint8_t modulo) {
  Grid *g = grid_create(w, h, 0, modulo, PACKED);
  MandelView v;
  dive_view(&v, dive_hour_seed(2026, 2026, 326, hour), minute, g->w, g->h);
  grid_start(g, &v);
  while (grid_step(g)) {
  }
  return g;
}

// Packed cells agree with the escape counts they were made from: the set is
// GRID_PACKED_INSIDE, lace is exactly where the count jumps from the left
// neighbour's, every other band is the count mod the palette, and depth
// never falls as the count rises.
static void test_packed(void) {
  const int w = 72, h = 84;
  for (int minute = 0; minute < 60; minute += 13) {
    Grid *g = computed(4, minute, w, h, 18);
    CHECK(g->depth_low <= g->depth_high, ":%02d depth quantiles %d..%d",
          minute, g->depth_low, g->depth_high);
    int wrong = 0, order = 0;
    uint8_t depth_at[2048];
    memset(depth_at, 0xFF, sizeof(depth_at));
    for (int y = 0; y < h; y++) {
      uint16_t left = 0;
      for (int x = 0; x < w; x++) {
        int32_t cr, ci;
        dive_point(&g->view, x, y, g->w, g->h, &cr, &ci);
        uint16_t n = mandel_escape(cr, ci, g->view.max_iter);
        uint8_t cell = g->cells[y * w + x];
        if (n == MANDEL_INSIDE) {
          wrong += cell != GRID_PACKED_INSIDE;
        } else {
          bool lace = x > 0 && (left == MANDEL_INSIDE || abs((int)n - (int)left) > 6);
          wrong += grid_band(cell) != (lace ? GRID_BAND_LACE : n % 18);
          uint8_t d = grid_depth(cell);
          if (depth_at[n] == 0xFF) depth_at[n] = d;
          order += depth_at[n] != d;  // one count, one depth
          wrong += (n <= g->depth_low && d != 0) || (n >= g->depth_high && d != GRID_DEPTH_MAX);
        }
        left = n;
      }
    }
    int last = 0;
    for (int n = 0; n < 2048; n++) {
      if (depth_at[n] == 0xFF) continue;
      order += depth_at[n] < last;
      last = depth_at[n];
    }
    CHECK(wrong == 0 && order == 0, ":%02d %d cells wrong, %d out of depth order", minute, wrong, order);
    grid_destroy(g);
  }
  CHECK(grid_create(8, 8, 0, GRID_BAND_LACE + 1, PACKED) == NULL,
        "a palette as long as the special bands is refused");
}

// Merging keeps the nearer distance and clips at every edge.
static void test_dist_merge(void) {
  DistField *d = dist_create(10, 6);  // samples: 6 x 4
  CHECK(d->w == 6 && d->h == 4, "10 x 6 px needs 6 x 4 samples (got %d x %d)", d->w, d->h);
  const uint8_t row[4] = {9, 3, 7, 1};
  dist_merge_row(d, -1, 0, row, 4);  // first sample off the left edge
  dist_merge_row(d, 4, 1, row, 4);   // last two off the right edge
  dist_merge_row(d, 0, -1, row, 4);  // whole row off the top
  dist_merge_row(d, 0, 4, row, 4);   // and off the bottom
  const uint8_t again[4] = {5, 5, 5, 5};
  dist_merge_row(d, -1, 0, again, 4);
  CHECK(d->d[0] == 3 && d->d[1] == 5 && d->d[2] == 1 && d->d[3] == FACE_DIST_FAR,
        "row 0: %d %d %d %d", d->d[0], d->d[1], d->d[2], d->d[3]);
  CHECK(d->d[6 + 4] == 9 && d->d[6 + 5] == 3, "row 1 clipped on the right");
  int far = 0;
  for (int i = 12; i < 24; i++) far += d->d[i] == FACE_DIST_FAR;
  CHECK(far == 12, "rows off the field leave it untouched");
  dist_destroy(d);
}

// Shading with a flat cell everywhere: the bilinear distance of a field that
// grows linearly is exact, so each pixel falls in the zone its distance says.
static void test_face_row(void) {
  const int w = 41;
  DistField *d = dist_create(w, 4);
  for (int j = 0; j < d->h; j++) {
    for (int i = 0; i < d->w; i++) {
      d->d[j * d->w + i] = (uint8_t)(8 * i);  // pixel x is x px from the line
    }
  }
  const uint8_t palette[4] = {10, 11, 12, 13};
  const FaceColors colors = {palette, 4, 1, 2, 3, 4};
  const FaceStyle style = {8, 0, 0, 4, 12, 4};  // radius 2 px, keyline 1, halo 3
  static FaceFrame frame;
  face_frame(&frame, &style, &colors, 0);
  uint8_t cells[41], out[41];
  memset(cells, 2, sizeof(cells));  // band 2, depth 0: radius is the base
  for (int y = 0; y < 4; y++) {
    face_row(&frame, cells, 0, d, y, 0, w - 1, out);
    int wrong = 0;
    for (int x = 0; x < w; x++) {
      uint8_t want = x < 2 ? 1 : x < 3 ? 2 : x < 6 ? palette[(4 - (x - 3)) % 4] : palette[2];
      wrong += out[x] != want;
    }
    CHECK(wrong == 0, "row %d: %d pixels in the wrong zone", y, wrong);
  }
  dist_destroy(d);
}

// The stroke radius follows the formula in face.h, and the tide and the
// halo both move one band outward per phase step.
static void test_face_frame(void) {
  uint8_t palette[18];
  for (int i = 0; i < 18; i++) palette[i] = (uint8_t)i;
  const FaceColors colors = {palette, 18, 100, 101, 102, 103};
  const FaceStyle style = {6, 18, 10, 5, 27, 6};
  static FaceFrame a, b;
  int wrong = 0, wave = 0, halo = 0;
  for (int phase = 0; phase < 18; phase++) {
    face_frame(&a, &style, &colors, (uint8_t)phase);
    face_frame(&b, &style, &colors, (uint8_t)((phase + 1) % 18));
    for (int depth = 0; depth <= GRID_DEPTH_MAX; depth++) {
      for (int band = 0; band < 18; band++) {
        int m = (band + phase) % 9;
        int t = 9 - abs(2 * m - 9);
        double want = 6 + 18.0 * depth * depth / 49 + 10.0 * t * t / 64;  // crest 8
        uint8_t cell = (uint8_t)(depth << GRID_BAND_BITS | band);
        wrong += fabs(a.radius[cell] - want) > 1.0;
        // Next phase, band b shows what band b + 1 showed: crests and colors
        // move to lower counts, away from the set.
        uint8_t inner = (uint8_t)(depth << GRID_BAND_BITS | (band + 1) % 18);
        wave += b.radius[cell] != a.radius[inner] || b.color[cell] != a.color[inner];
      }
    }
    // Next phase, each halo band shows what the band inside it showed.
    for (int q = 6; q < 27; q++) {
      halo += b.halo_color[q] != a.halo_color[q - 6];
    }
  }
  CHECK(wrong == 0, "%d radii off the formula", wrong);
  CHECK(wave == 0, "%d cells where the tide does not ride the bands", wave);
  CHECK(halo == 0, "%d halo bands that do not flow outward", halo);
  face_frame(&a, &style, &colors, 0);
  CHECK(a.radius[GRID_PACKED_INSIDE] == 6 + 18 + 10 && a.color[GRID_PACKED_INSIDE] == 102,
        "the set: widest strokes, inside color");
  uint8_t lace = 3 << GRID_BAND_BITS | GRID_BAND_LACE;
  CHECK(a.radius[lace] == 6 + 3 && a.color[lace] == 103,  // 18 * 3^2 / 7^2 = 3.3
        "lace: its depth, no tide, lace color (radius %d)", a.radius[lace]);
}

int main(void) {
  test_matches_double();
  test_known_points();
  test_dive();
  test_interlace();
  test_packed();
  test_dist_merge();
  test_face_row();
  test_face_frame();
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
