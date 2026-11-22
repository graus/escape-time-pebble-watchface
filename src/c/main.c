// Escape Time: the time in four large digits, each a window onto a dive into
// the Mandelbrot set. The hour picks where to dive, the minute how deep.

#include <pebble.h>

#include "dive.h"
#include "generated/palette.h"
#include "grid.h"
#include "layout.h"
#include "paint.h"

// Palette steps per second: how fast the bands flow.
#define FPS 10
// Longest a frame may spend computing the new view, so the face stays
// responsive (the rest waits for the next frame).
#define COMPUTE_BUDGET_MS 40

static const uint32_t s_digit_resources[2][10] = {
  [DigitSetLarge] = {
    RESOURCE_ID_DIGIT_0, RESOURCE_ID_DIGIT_1, RESOURCE_ID_DIGIT_2, RESOURCE_ID_DIGIT_3,
    RESOURCE_ID_DIGIT_4, RESOURCE_ID_DIGIT_5, RESOURCE_ID_DIGIT_6, RESOURCE_ID_DIGIT_7,
    RESOURCE_ID_DIGIT_8, RESOURCE_ID_DIGIT_9,
  },
  [DigitSetCompact] = {
    RESOURCE_ID_DIGIT_S_0, RESOURCE_ID_DIGIT_S_1, RESOURCE_ID_DIGIT_S_2, RESOURCE_ID_DIGIT_S_3,
    RESOURCE_ID_DIGIT_S_4, RESOURCE_ID_DIGIT_S_5, RESOURCE_ID_DIGIT_S_6, RESOURCE_ID_DIGIT_S_7,
    RESOURCE_ID_DIGIT_S_8, RESOURCE_ID_DIGIT_S_9,
  },
};

static Window *s_window;
static Layer *s_canvas;

static GBitmap *s_digits[4];
static uint8_t s_digit_values[4] = {0xFF, 0xFF, 0xFF, 0xFF};  // loaded bitmaps
static uint8_t s_time_digits[4];                                // the time to show
static DigitSet s_digit_set = DigitSetLarge;
static Layout s_layout;

static Grid *s_grid;
static GSize s_grid_window;  // window size the grid was built for
static int s_hour, s_minute;  // the view being shown

static AppTimer *s_frame_timer;
static bool s_focused = true;
static uint8_t s_phase;

static uint32_t s_compute_start;

static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000 + ms;
}

// ---------------------------------------------------------------------------
// The fractal

static void start_view(void) {
  if (!s_grid || s_hour < 0) {
    return;  // no grid, or the time is not known yet
  }
  MandelView view;
  dive_view(&view, s_hour, s_minute, s_grid->w, s_grid->h);
  grid_start(s_grid, &view);
  s_compute_start = now_ms();
}

// Color watches try 1 px cells, then coarser ones if the heap is short; B&W
// watches always use 2 px cells (four to a framebuffer byte).
static Grid *create_grid(GSize window) {
#if defined(PBL_COLOR)
  for (uint8_t shift = 0; shift <= 2; shift++) {
#else
  for (uint8_t shift = 1; shift <= 1; shift++) {
#endif
    Grid *grid = grid_create(window.w, window.h, shift, PALETTE_SIZE);
    if (grid) {
      return grid;
    }
  }
  return NULL;
}

// Replaces the grid with one for the current window (it changes with the
// digit set, when Timeline Quick View comes and goes). The old grid is freed
// first: two full grids may not fit side by side.
static void rebuild_grid(void) {
  grid_destroy(s_grid);
  s_grid = create_grid(s_layout.window.size);
  s_grid_window = s_layout.window.size;
  if (s_grid) {
    APP_LOG(APP_LOG_LEVEL_INFO, "grid %dx%d, %d px cells, heap free %d",
            s_grid->w, s_grid->h, 1 << s_grid->shift, (int)heap_bytes_free());
    start_view();
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "no memory for the grid");
  }
}

// Computes rows of the new view until the frame's budget is spent.
static void compute(void) {
  if (!s_grid || grid_done(s_grid)) {
    return;
  }
  uint32_t start = now_ms();
  while (grid_step(s_grid)) {
    if (now_ms() - start >= COMPUTE_BUDGET_MS) {
      return;
    }
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "%02d:%02d view computed in %d ms",
          s_hour, s_minute, (int)(now_ms() - s_compute_start));
}

// ---------------------------------------------------------------------------
// Digits and layout

static void set_digit(int index, uint8_t value) {
  if (s_digit_values[index] == value) {
    return;  // only changed digits are reloaded
  }
  GBitmap *bitmap = gbitmap_create_with_resource(s_digit_resources[s_digit_set][value]);
  if (!bitmap) {
    return;  // keep the old digit rather than show a hole
  }
  gbitmap_destroy(s_digits[index]);
  s_digits[index] = bitmap;
  s_digit_values[index] = value;
}

// Picks the digit set for the visible area (large, or compact while Timeline
// Quick View covers the bottom of the screen), loads the time's digits in it,
// and lays them out.
static void update_layout(void) {
  GRect area = layer_get_unobstructed_bounds(s_canvas);
  DigitSet set = layout_choose_set(area.size.h);
  if (set != s_digit_set) {
    s_digit_set = set;
    for (int i = 0; i < 4; i++) {
      s_digit_values[i] = 0xFF;  // force a reload in the new set
    }
  }
  for (int i = 0; i < 4; i++) {
    set_digit(i, s_time_digits[i]);
  }
  GSize sizes[4];
  for (int i = 0; i < 4; i++) {
    sizes[i] = s_digits[i] ? gbitmap_get_bounds(s_digits[i]).size : GSize(0, 0);
  }
  GRect bounds = layer_get_bounds(s_canvas);
  layout_compute(&s_layout, s_digit_set, area, bounds.size.w, sizes);
  if (!gsize_equal(&s_layout.window.size, &s_grid_window)) {
    rebuild_grid();
  }
}

static void update_time(struct tm *t) {
  int hour = t->tm_hour;
  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) hour = 12;
  }
  // Always four digits ("0907"), so the layout never jumps.
  s_time_digits[0] = hour / 10;
  s_time_digits[1] = hour % 10;
  s_time_digits[2] = t->tm_min / 10;
  s_time_digits[3] = t->tm_min % 10;
  update_layout();  // loads changed digits (all of them if the set changed)

  // The dive follows the 24-hour clock whatever the display style.
  if (t->tm_hour != s_hour || t->tm_min != s_minute) {
    s_hour = t->tm_hour;
    s_minute = t->tm_min;
    start_view();
  }
  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Drawing

static void canvas_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  for (int i = 0; i < 4; i++) {
    if (s_digits[i]) {
      graphics_draw_bitmap_in_rect(ctx, s_digits[i], s_layout.digits[i]);
    }
  }
  if (!s_grid) {
    return;  // plain white digits rather than nothing
  }
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    paint(fb, s_grid, s_layout.window, s_phase);
    graphics_release_frame_buffer(ctx, fb);
  }
}

// ---------------------------------------------------------------------------
// Animation

static void frame(void *context) {
  // Schedule first so the frame rate does not drift by the cost of a frame.
  s_frame_timer = app_timer_register(1000 / FPS, frame, NULL);
  compute();
  // A cell shows palette entry (count + phase): as the phase grows, each
  // color moves to the cells with one iteration less, outward from the set.
  s_phase = (s_phase + 1) % PALETTE_SIZE;
  layer_mark_dirty(s_canvas);
}

static void update_running(void) {
  if (s_focused && !s_frame_timer) {
    s_frame_timer = app_timer_register(1000 / FPS, frame, NULL);
  } else if (!s_focused && s_frame_timer) {
    app_timer_cancel(s_frame_timer);
    s_frame_timer = NULL;
  }
}

// ---------------------------------------------------------------------------
// System events

static void tick_handler(struct tm *t, TimeUnits changed) {
  update_time(t);
}

static void focus_handler(bool in_focus) {
  s_focused = in_focus;
  update_running();
  if (in_focus) {
    layer_mark_dirty(s_canvas);  // the notification left the digits unpainted
  }
}

#if !defined(PBL_PLATFORM_APLITE)
static void unobstructed_did_change(void *context) {
  update_layout();
  layer_mark_dirty(s_canvas);
}
#endif

// ---------------------------------------------------------------------------
// Lifecycle

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  s_hour = -1;  // force the first view
  time_t now = time(NULL);
  update_time(localtime(&now));  // also lays out and builds the grid
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, false);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = focus_handler,
  });
#if !defined(PBL_PLATFORM_APLITE)
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers){
    .did_change = unobstructed_did_change,
  }, NULL);
#endif
  update_running();
  APP_LOG(APP_LOG_LEVEL_INFO, "started, heap free %d", (int)heap_bytes_free());
}

static void deinit(void) {
  if (s_frame_timer) app_timer_cancel(s_frame_timer);
  tick_timer_service_unsubscribe();
  app_focus_service_unsubscribe();
  window_destroy(s_window);
  for (int i = 0; i < 4; i++) {
    gbitmap_destroy(s_digits[i]);
  }
  grid_destroy(s_grid);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
