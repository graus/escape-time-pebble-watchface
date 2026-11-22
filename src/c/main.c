// Escape Time: the time in four large digits and a dive into the Mandelbrot
// set. The hour picks where to dive, the minute how deep.
//
// The dive fills the screen and the digits are part of it: white strokes
// that count as points in the set, so the bands flow out of them; their
// weight follows the depth of the dive and swells with the moving palette
// (face.h). B&W watches show the same face, dithered.

#include <pebble.h>

#include "dive.h"
#include "face.h"
#include "generated/glyphs.h"
#include "generated/palette.h"
#include "grid.h"
#include "layout.h"
#include "paint.h"

// Longest a frame may spend computing the new view, so the face stays
// responsive (the rest waits for the next frame).
#define COMPUTE_BUDGET_MS 40

static const uint32_t s_glyph_resources[2][10] = {
  [DigitSetLarge] = {
    RESOURCE_ID_GLYPH_0, RESOURCE_ID_GLYPH_1, RESOURCE_ID_GLYPH_2, RESOURCE_ID_GLYPH_3,
    RESOURCE_ID_GLYPH_4, RESOURCE_ID_GLYPH_5, RESOURCE_ID_GLYPH_6, RESOURCE_ID_GLYPH_7,
    RESOURCE_ID_GLYPH_8, RESOURCE_ID_GLYPH_9,
  },
  [DigitSetCompact] = {
    RESOURCE_ID_GLYPH_S_0, RESOURCE_ID_GLYPH_S_1, RESOURCE_ID_GLYPH_S_2, RESOURCE_ID_GLYPH_S_3,
    RESOURCE_ID_GLYPH_S_4, RESOURCE_ID_GLYPH_S_5, RESOURCE_ID_GLYPH_S_6, RESOURCE_ID_GLYPH_S_7,
    RESOURCE_ID_GLYPH_S_8, RESOURCE_ID_GLYPH_S_9,
  },
};

typedef struct {
  uint8_t cols, rows, pad;
} GlyphField;

static const GlyphField s_glyph_fields[2] = {
  [DigitSetLarge] = {GLYPH_L_COLS, GLYPH_L_ROWS, GLYPH_L_PAD},
  [DigitSetCompact] = {GLYPH_S_COLS, GLYPH_S_ROWS, GLYPH_S_PAD},
};

#define GLYPH_MAX_COLS (GLYPH_L_COLS > GLYPH_S_COLS ? GLYPH_L_COLS : GLYPH_S_COLS)

static const FaceStyle s_styles[2] = {
  [DigitSetLarge] = GLYPH_L_STYLE,
  [DigitSetCompact] = GLYPH_S_STYLE,
};

static const FaceColors s_colors = {
  .palette = s_palette,
  .size = PALETTE_SIZE,
  .digit = FACE_DIGIT,
  .keyline = FACE_KEYLINE,
  .inside = FACE_INSIDE,
  .lace = FACE_LACE,
};

static Window *s_window;
static Layer *s_canvas;

static uint8_t s_time_digits[4];  // the time to show
static DigitSet s_digit_set = DigitSetLarge;
static Layout s_layout;

static DistField *s_dist;
static uint8_t s_dist_digits[4] = {0xFF, 0xFF, 0xFF, 0xFF};  // what s_dist holds
static FaceFrame s_frame;

static Grid *s_grid;
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

// The dive covers the screen in packed cells (depth for the digits' weight,
// lace for filaments): 1 px cells, or coarser ones if the heap is short (as
// on aplite).
static void create_grid(GSize screen) {
  const GridEncoding encoding = {
    .lace_step = LACE_STEP,
    .low_percent = DEPTH_LOW_PERCENT,
    .high_percent = DEPTH_HIGH_PERCENT,
  };
  for (uint8_t shift = 0; shift <= 2 && !s_grid; shift++) {
    s_grid = grid_create(screen.w, screen.h, shift, PALETTE_SIZE, encoding);
  }
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

// Merges a digit's distance field, read a row at a time, into s_dist at its
// cell (on even pixels, as is the field's margin, so samples line up).
static void merge_glyph(DigitSet set, uint8_t value, GPoint cell) {
  const GlyphField *field = &s_glyph_fields[set];
  ResHandle handle = resource_get_handle(s_glyph_resources[set][value]);
  int i = (cell.x - field->pad) / 2, j = (cell.y - field->pad) / 2;
  uint8_t row[GLYPH_MAX_COLS];
  for (int r = 0; r < field->rows; r++) {
    resource_load_byte_range(handle, r * field->cols, row, field->cols);
    dist_merge_row(s_dist, i, j + r, row, field->cols);
  }
}

// Picks the digit set for the visible area (large, or compact while Timeline
// Quick View covers the bottom of the screen), lays the digits out, and
// rebuilds the distance field when the digits or their places changed.
static void update_layout(void) {
  GRect area = layer_get_unobstructed_bounds(s_canvas);
  GSize screen = layer_get_bounds(s_canvas).size;
  DigitSet set = layout_choose_set(area.size.h);
  Layout layout;
  layout_compute(&layout, set, area, screen.w);

  bool moved = set != s_digit_set || memcmp(&layout, &s_layout, sizeof(layout)) != 0;
  bool changed = moved || memcmp(s_time_digits, s_dist_digits, 4) != 0;
  s_digit_set = set;
  s_layout = layout;
  if (s_dist && changed) {
    dist_clear(s_dist);
    for (int i = 0; i < 4; i++) {
      merge_glyph(set, s_time_digits[i], s_layout.digits[i].origin);
    }
    memcpy(s_dist_digits, s_time_digits, 4);
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
  update_layout();

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
  if (!s_grid || !s_dist) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    return;  // out of memory: logged when it happened
  }
  // Every pixel is shaded, so nothing needs clearing first.
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    face_frame(&s_frame, &s_styles[s_digit_set], &s_colors, s_phase);
    paint_face(fb, &s_frame, s_grid, s_dist);
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
    layer_mark_dirty(s_canvas);  // the notification left the face unpainted
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

  // The distance field first: it is small, and the grid takes what is left.
  GSize screen = layer_get_bounds(s_canvas).size;
  s_dist = dist_create(screen.w, screen.h);
  if (!s_dist) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "no memory for the digits");
  }
  create_grid(screen);

  s_hour = -1;  // force the first view
  time_t now = time(NULL);
  update_time(localtime(&now));
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
  dist_destroy(s_dist);
  grid_destroy(s_grid);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
