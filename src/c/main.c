// Escape Time: the time in four large digits and a dive into the Mandelbrot
// set. The hour picks where to dive, the minute how deep.
//
// The dive fills the screen and the digits are part of it: white strokes
// that count as points in the set, so the bands flow out of them; their
// weight follows the depth of the dive and swells with the moving palette
// (face.h). B&W watches show the same face, dithered. Settings, from the
// phone, pick its look, light (white digits) or dark (black digits over a
// darker palette), and can switch to dark for set night hours.
//
// To save the battery the face is still most of the time: frames run only
// while a new minute's view is being computed (it sharpens in, then stops)
// and for AWAKE_MS after a tap (a flick of the wrist, or a touch on Pebble
// Time 2 and Round 2), when the colors flow. While still, nothing runs but
// the minute tick.
//
// Battery saver (a setting) never animates: a new minute's view is computed
// unseen and painted once, whole, with the minute's digits, in one update a
// minute; taps change nothing.

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
// How long the colors flow after a tap (and after the face starts).
#define AWAKE_MS 30000
// In battery saver, the pause between a view's compute steps: just enough
// for the system's events, as nothing is painted until the view is done.
#define SAVER_YIELD_MS 10

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

// The face's looks (tools/design.py MODES), filled in from the generated
// tables when the face starts.
static FaceColors s_colors[FACE_MODES];

static void init_colors(void) {
  for (int m = 0; m < FACE_MODES; m++) {
    s_colors[m] = (FaceColors){
      .palette = s_palettes[m],
      .size = PALETTE_SIZE,
      .digit = s_face_colors[m][0],
      .keyline = s_face_colors[m][1],
      .inside = s_face_colors[m][2],
      .lace = s_face_colors[m][3],
    };
  }
}

// Settings, kept on the watch so the face needs the phone only to change
// them. (Key 1 is unused: a pre-release build's.)
enum {
  PERSIST_KEY_MODE = 2,
  PERSIST_KEY_NIGHT,
  PERSIST_KEY_NIGHT_START,
  PERSIST_KEY_NIGHT_END,
  PERSIST_KEY_WATCH_SEED,  // this watch's own, made once: its dives are its own
  PERSIST_KEY_SAVER,
};
static uint8_t s_mode = FACE_MODE_LIGHT;  // the chosen look
static bool s_night;                      // dark from s_night_start to s_night_end
static uint8_t s_night_start = 22, s_night_end = 7;  // hours, 0-23
static bool s_saver;                      // battery saver: never animate

static Window *s_window;
static Layer *s_canvas;

static uint8_t s_time_digits[4];  // the time to show
static DigitSet s_digit_set = DigitSetLarge;
static Layout s_layout;

static DistField *s_dist;
static uint8_t s_dist_digits[4] = {0xFF, 0xFF, 0xFF, 0xFF};  // what s_dist holds
static FaceFrame s_frame;

static Grid *s_grid;
static int s_hour = -1, s_minute = -1;  // the view being shown (none yet)
static uint32_t s_watch_seed;             // see load_watch_seed
static uint32_t s_hour_seed;              // the dive of s_hour, today

static AppTimer *s_frame_timer;
static AppTimer *s_awake_timer;
static bool s_focused = true;
static bool s_awake;  // the colors flow: for AWAKE_MS after a tap
static uint8_t s_phase;

static uint32_t s_compute_start;

// Log the average cost of painting a frame once, after this many frames.
#define PROFILE_FRAMES 50
static uint32_t s_profile_frames, s_profile_ms;

static uint32_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint32_t)s * 1000 + ms;
}

// ---------------------------------------------------------------------------
// The fractal

static void update_running(void);
static bool computing(void);

static void start_view(void) {
  if (!s_grid || s_hour < 0) {
    return;  // no grid, or the time is not known yet
  }
  MandelView view;
  dive_view(&view, s_hour_seed, s_minute, s_grid->w, s_grid->h);
  grid_start(s_grid, &view);
  s_compute_start = now_ms();
  update_running();  // frames compute the view, even on a still face
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
    // A short read would merge stale stack bytes as distances: skip the row.
    if (resource_load_byte_range(handle, r * field->cols, row, field->cols) != field->cols) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "glyph %d row %d: short read", value, r);
      continue;
    }
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
  // Each hour of each day dives anew, as only this watch does.
  uint32_t seed = dive_hour_seed(s_watch_seed, t->tm_year + 1900, t->tm_yday, t->tm_hour);
  if (seed != s_hour_seed || t->tm_hour != s_hour || t->tm_min != s_minute) {
    s_hour_seed = seed;
    s_hour = t->tm_hour;
    s_minute = t->tm_min;
    start_view();
  }
  // In battery saver the digits wait for the new view (a fraction of a
  // second), so both change in one paint.
  if (!(s_saver && computing())) {
    layer_mark_dirty(s_canvas);
  }
}

// ---------------------------------------------------------------------------
// Drawing

// Whether an hour (0-23) falls in the night hours: from the start up to the
// end, over midnight when the start is later. Equal hours: no night.
static bool is_night_hour(int hour) {
  if (s_night_start <= s_night_end) {
    return hour >= s_night_start && hour < s_night_end;
  }
  return hour >= s_night_start || hour < s_night_end;
}

// The look to paint now: dark during the night hours, if set, else the
// chosen one. The minute tick repaints, so the switch comes on the hour.
static uint8_t shown_mode(void) {
  if (s_night && s_hour >= 0 && is_night_hour(s_hour)) {
    return FACE_MODE_DARK;
  }
  return s_mode;
}

// Without memory for the dive, the digits are still drawn, over a flat grid
// of points in the set: 2x2 cells of 256 px cover any screen, so this costs
// four bytes. The digits and their keyline on black: the time, not nothing.
static uint8_t s_flat_cells[4] = {
  GRID_PACKED_INSIDE, GRID_PACKED_INSIDE, GRID_PACKED_INSIDE, GRID_PACKED_INSIDE,
};
static const Grid s_flat_grid = {.w = 2, .h = 2, .shift = 8, .cells = s_flat_cells};

static void canvas_update(Layer *layer, GContext *ctx) {
  if (!s_dist) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    return;  // out of memory even for the digits: logged when it happened
  }
  // Every pixel is shaded, so nothing needs clearing first.
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    uint32_t start = now_ms();
    face_frame(&s_frame, &s_styles[s_digit_set], &s_colors[shown_mode()], s_phase);
    paint_face(fb, &s_frame, s_grid ? s_grid : &s_flat_grid, s_dist);
    graphics_release_frame_buffer(ctx, fb);
    if (s_profile_frames < PROFILE_FRAMES) {
      s_profile_ms += now_ms() - start;
      if (++s_profile_frames == PROFILE_FRAMES) {
        APP_LOG(APP_LOG_LEVEL_INFO, "paint: %d.%02d ms per frame",
                (int)(s_profile_ms / PROFILE_FRAMES), (int)(s_profile_ms % PROFILE_FRAMES * 100 / PROFILE_FRAMES));
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Animation

static bool computing(void) {
  return s_grid && !grid_done(s_grid);
}

// Frames run while the face is in front and either awake or still
// computing the minute's view.
static bool wants_frames(void) {
  return s_focused && ((s_awake && !s_saver) || computing());
}

// Time from one frame's start to the next's (in battery saver, which
// paints nothing in between: the pause after a step, so the view is done
// sooner).
static uint32_t frame_period(void) {
  return s_saver ? SAVER_YIELD_MS : 1000 / FPS;
}

static void frame(void *context) {
  uint32_t start = now_ms();
  compute();
  if (!s_saver) {
    // A cell shows palette entry (count + phase): as the phase grows, each
    // color moves to the cells with one iteration less, outward from the
    // set.
    s_phase = (s_phase + 1) % PALETTE_SIZE;
    layer_mark_dirty(s_canvas);
  } else if (!computing()) {
    layer_mark_dirty(s_canvas);  // the finished view, whole, in one paint
  }
  // The last frame paints the finished view; after it the face stays still.
  if (wants_frames()) {
    // Animating, frames keep the FPS pace; in battery saver each compute
    // step (its full budget, usually) is followed by a real pause.
    uint32_t spent = now_ms() - start;
    uint32_t period = frame_period();
    uint32_t delay = s_saver ? SAVER_YIELD_MS : spent < period ? period - spent : 1;
    s_frame_timer = app_timer_register(delay, frame, NULL);
  } else {
    s_frame_timer = NULL;
    APP_LOG(APP_LOG_LEVEL_INFO, "still");
  }
}

static void update_running(void) {
  if (wants_frames() && !s_frame_timer) {
    s_frame_timer = app_timer_register(frame_period(), frame, NULL);
  } else if (!s_focused && s_frame_timer) {
    app_timer_cancel(s_frame_timer);
    s_frame_timer = NULL;
  }
}

static void awake_timeout(void *context) {
  s_awake_timer = NULL;
  s_awake = false;  // the running frame stops itself once the view is done
}

// Sets the colors flowing for AWAKE_MS (again); not in battery saver.
static void wake(void) {
  if (s_saver) {
    return;
  }
  s_awake = true;
  if (s_awake_timer) {
    app_timer_reschedule(s_awake_timer, AWAKE_MS);
  } else {
    s_awake_timer = app_timer_register(AWAKE_MS, awake_timeout, NULL);
  }
  update_running();
}

// The accelerometer's tap: a sharp flick of the wrist or a knock on the
// watch. The tap service is event driven, so listening costs nothing.
static void tap_handler(AccelAxisType axis, int32_t direction) {
  wake();
}

#if defined(PBL_TOUCH)
// On a touchscreen (Pebble Time 2, Round 2) a finger on the screen is the
// natural tap; the accelerometer does not register it.
static void touch_handler(const TouchEvent *event, void *context) {
  if (event->type == TouchEvent_Touchdown) {
    wake();
  }
}
#endif

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
// Settings

// A setting's value, whatever integer width the phone sent.
static int32_t tuple_int(const Tuple *t) {
  switch (t->type) {
    case TUPLE_INT:
      return t->length == 1 ? t->value->int8 : t->length == 2 ? t->value->int16 : t->value->int32;
    case TUPLE_UINT:
      return t->length == 1 ? t->value->uint8 : t->length == 2 ? t->value->uint16 : (int32_t)t->value->uint32;
    default:
      return 0;
  }
}

static uint8_t clamp_hour(int32_t hour) {
  return hour < 0 ? 0 : hour > 23 ? 23 : (uint8_t)hour;
}

// The phone's settings page (src/pkjs) sends all its values when saved.
static void inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *t;
  if ((t = dict_find(iter, MESSAGE_KEY_Mode))) {
    int32_t mode = tuple_int(t);
    s_mode = mode >= 0 && mode < FACE_MODES ? (uint8_t)mode : FACE_MODE_LIGHT;
    persist_write_int(PERSIST_KEY_MODE, s_mode);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_NightDark))) {
    s_night = tuple_int(t) != 0;
    persist_write_bool(PERSIST_KEY_NIGHT, s_night);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_NightStart))) {
    s_night_start = clamp_hour(tuple_int(t));
    persist_write_int(PERSIST_KEY_NIGHT_START, s_night_start);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_NightEnd))) {
    s_night_end = clamp_hour(tuple_int(t));
    persist_write_int(PERSIST_KEY_NIGHT_END, s_night_end);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Saver))) {
    s_saver = tuple_int(t) != 0;
    persist_write_bool(PERSIST_KEY_SAVER, s_saver);
    if (s_saver) {
      s_awake = false;  // flowing colors stop now; the running frame stops itself
    }
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "settings: mode %d, night %s %02d-%02d, saver %s, showing %d",
          s_mode, s_night ? "on" : "off", s_night_start, s_night_end, s_saver ? "on" : "off",
          shown_mode());
  if (s_canvas) {
    layer_mark_dirty(s_canvas);
  }
}

// The watch's own seed, so its dives are unlike any other watch's: made
// once, from the moment the face first ran, and kept.
static void load_watch_seed(void) {
  if (persist_exists(PERSIST_KEY_WATCH_SEED)) {
    s_watch_seed = (uint32_t)persist_read_int(PERSIST_KEY_WATCH_SEED);
    return;
  }
  time_t now;
  uint16_t ms;
  time_ms(&now, &ms);
  srand((uint32_t)now * 1000u + ms);  // unsigned: now * 1000 overflows int32
  s_watch_seed = ((uint32_t)rand() << 16) ^ (uint32_t)rand() ^ (uint32_t)now * 2654435761u ^ ms;
  persist_write_int(PERSIST_KEY_WATCH_SEED, (int32_t)s_watch_seed);
  APP_LOG(APP_LOG_LEVEL_INFO, "a new watch seed: %08lx", (unsigned long)s_watch_seed);
}

static void load_settings(void) {
  if (persist_exists(PERSIST_KEY_MODE)) {
    int32_t mode = persist_read_int(PERSIST_KEY_MODE);
    s_mode = mode >= 0 && mode < FACE_MODES ? (uint8_t)mode : FACE_MODE_LIGHT;
  }
  s_night = persist_exists(PERSIST_KEY_NIGHT) && persist_read_bool(PERSIST_KEY_NIGHT);
  if (persist_exists(PERSIST_KEY_NIGHT_START)) {
    s_night_start = clamp_hour(persist_read_int(PERSIST_KEY_NIGHT_START));
  }
  if (persist_exists(PERSIST_KEY_NIGHT_END)) {
    s_night_end = clamp_hour(persist_read_int(PERSIST_KEY_NIGHT_END));
  }
  s_saver = persist_exists(PERSIST_KEY_SAVER) && persist_read_bool(PERSIST_KEY_SAVER);
}

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

  time_t now = time(NULL);
  update_time(localtime(&now));
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  init_colors();
  load_settings();
  load_watch_seed();
  // Before the window: its grid takes what is left of the heap. The settings
  // are a few small values, and the face sends nothing.
  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 0);

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
  accel_tap_service_subscribe(tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_handler, NULL);
#endif
  wake();  // the colors flow for a while when the face appears (not in saver)
  APP_LOG(APP_LOG_LEVEL_INFO, "started, heap free %d", (int)heap_bytes_free());
}

static void deinit(void) {
  if (s_frame_timer) app_timer_cancel(s_frame_timer);
  if (s_awake_timer) app_timer_cancel(s_awake_timer);
  accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  tick_timer_service_unsubscribe();
  app_focus_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
  dist_destroy(s_dist);
  grid_destroy(s_grid);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
