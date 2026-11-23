#include "dive.h"

#include "generated/targets.h"  // s_targets: DIVE_TARGETS Misiurewicz points
#include "generated/trig.h"     // s_quarter_sine: a quarter wave, Q14

// The whole set, which spans about -2..0.47 by -1.12..1.12: within 1.45 of
// its middle, so a 2.9 wide circle holds it at any angle.
#define HOME_R MANDEL_FIX(-0.75)
#define HOME_I 0
#define HOME_SPAN MANDEL_FIX(2.9)

// Extra fraction bits while zooming, so 59 steps of 5/6 do not round away.
// (Multiplied in, not shifted: shifting a negative value left is undefined.)
#define EXTRA 16
#define WIDEN(v) ((int64_t)(v) * ((int64_t)1 << EXTRA))
#define NARROW(v) ((int32_t)((v) / ((int64_t)1 << EXTRA)))

// v / d rounded to the nearest (C division truncates toward zero).
static int64_t divide_rounded(int64_t v, int64_t d) {
  return (v >= 0 ? v + d / 2 : v - d / 2) / d;
}

// Iterations: deeper views need more before their detail escapes.
#define BASE_ITER 64
#define ITER_PER_MINUTE 6

// Angles are 1/65536 of a turn. Each hour spins at a random rate of up to
// SPIN_MAX per minute either way, and each minute adds a random nudge of up
// to NUDGE_MAX either way.
#define DEGREES(d) ((int32_t)((d) * 65536L / 360))
#define SPIN_MAX DEGREES(4)
#define NUDGE_MAX DEGREES(2)

// MurmurHash3's finalizer: every input bit flips half the output bits.
static uint32_t mix(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85EBCA6Bu;
  h ^= h >> 13;
  h *= 0xC2B2AE35u;
  h ^= h >> 16;
  return h;
}

uint32_t dive_hour_seed(uint32_t watch_seed, int year, int yday, int hour) {
  uint32_t when = (uint32_t)((year * 366 + yday) * 24 + hour);
  return mix(watch_seed ^ mix(when + 0x9E3779B9u));
}

// xorshift32: small and fast, and plenty for an angle a minute.
static uint32_t next_random(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return *state = x;
}

// A random integer in -max..max.
static int32_t random_signed(uint32_t *state, int32_t max) {
  return (int32_t)(next_random(state) % (uint32_t)(2 * max + 1)) - max;
}

// sin of an angle (1/65536 of a turn), Q14, from the quarter wave.
static int32_t sine(uint16_t angle) {
  int i = (angle >> 6) & 0xFF;  // 256 steps per quarter turn
  switch (angle >> 14) {
    case 0: return s_quarter_sine[i];
    case 1: return s_quarter_sine[256 - i];
    case 2: return -s_quarter_sine[i];
    default: return -s_quarter_sine[256 - i];
  }
}

void dive_view(MandelView *view, uint32_t seed, int minute, int w, int h) {
  if (minute < 0) minute = 0;
  if (minute > 59) minute = 59;
  uint32_t rng = mix(seed) | 1;  // xorshift's state must not be 0

  // The hour's target, and whether to dive into its mirror image.
  uint32_t pick = next_random(&rng);
  const int32_t *t = s_targets[pick % DIVE_TARGETS];
  int32_t tr = t[0], ti = (pick >> 16) & 1 ? -t[1] : t[1];

  // The angle: upright at :00, then the hour's spin and a nudge a minute.
  int32_t spin = random_signed(&rng, SPIN_MAX);
  int32_t angle = 0;
  for (int m = 0; m < minute; m++) {
    angle += spin + random_signed(&rng, NUDGE_MAX);
  }

  // The step that fits the whole set on the screen's shorter side at :00,
  // then zooming about the target: the center's offset from it shrinks
  // with the step, and turns with the view, so the target keeps its place
  // on the screen and the view turns about it.
  int side = w < h ? w : h;
  int64_t step = WIDEN(HOME_SPAN / (side > 0 ? side : 1));
  int64_t off_r = WIDEN(HOME_R - tr), off_i = WIDEN(HOME_I - ti);
  for (int m = 0; m < minute; m++) {
    step = step * 5 / 6;
    off_r = off_r * 5 / 6;
    off_i = off_i * 5 / 6;
  }
  uint16_t a = (uint16_t)angle;
  const int64_t c = sine((uint16_t)(a + 16384)), s = sine(a);  // Q14
  const int64_t one = (int64_t)16384 << EXTRA;
  // A screen offset (dx, dy) is the complex (dx - i dy) * step * e^(i angle)
  // (dive_point), so the offset turns by e^(i angle) too.
  int64_t r = NARROW(off_r), i = NARROW(off_i);
  view->cr = tr + (int32_t)divide_rounded(r * c - i * s, 16384);
  view->ci = ti + (int32_t)divide_rounded(r * s + i * c, 16384);
  view->step = NARROW(step);
  view->ux = (int32_t)divide_rounded(step * c, one);  // rounded, not truncated:
  view->uy = (int32_t)divide_rounded(step * s, one);  // a deep step is ~50 units
  view->max_iter = BASE_ITER + ITER_PER_MINUTE * minute;
}
