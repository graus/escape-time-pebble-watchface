// Host tests for the fractal engine: the watch's mandel.c, dive.c and
// grid.c, compiled natively.
//
//   make -C tools/host test

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/c/dive.h"
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

// :00 shows the whole set around the home center; deeper minutes close in
// on the target, which stays put.
static void test_dive(void) {
  MandelView v0, v59;
  dive_view(&v0, 0, 0, 135, 146);
  dive_view(&v59, 0, 59, 135, 146);
  CHECK(abs(v0.cr - MANDEL_FIX(-0.75)) < 2 && v0.ci == 0, "home center at :00");
  double zoom = (double)v0.step / v59.step;
  CHECK(zoom > 40000 && zoom < 55000, "zoom at :59 is (6/5)^59 = 47,000 (got %.0f)", zoom);
  CHECK(v59.step >= 32, "a pixel at :59 still spans %d units of 2^-27", v59.step);
  // Seahorse Valley is hour 0's target; the center has converged on it.
  CHECK(fabs(v59.cr / (double)MANDEL_ONE + 0.743643887) < 1e-4 &&
        fabs(v59.ci / (double)MANDEL_ONE - 0.131825904) < 1e-4, "hour 0 dives into Seahorse Valley");
  MandelView odd;
  dive_view(&odd, 1, 59, 135, 146);
  CHECK(odd.ci < 0, "odd hours dive into the mirror image");
  dive_view(&odd, 13, 59, 135, 146);
  MandelView one;
  dive_view(&one, 1, 59, 135, 146);
  CHECK(odd.cr == one.cr && odd.ci == one.ci, "13:00 repeats 1:00");
}

// Every row is computed exactly once, whatever the height.
static void test_interlace(void) {
  for (int h = 1; h <= 40; h++) {
    Grid *g = grid_create(8, h, 0, 24);
    MandelView v = {MANDEL_FIX(3.0), 0, 1, 1};  // far outside: escapes at once
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

int main(void) {
  test_matches_double();
  test_known_points();
  test_dive();
  test_interlace();
  if (failures) {
    printf("%d failure(s)\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
