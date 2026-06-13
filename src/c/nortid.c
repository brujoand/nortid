#include <pebble.h>
#include <stdlib.h>

#include "date2words.h"
#include "lang/lang.h"
#include "time2words.h"

static TextLayer* time_layer;
static Window* window;
static GFont date_font;
static GFont hr_font;

#define TIME_FONT_COUNT 8
// Index of the first font usable in normal (wrapping) mode. Entries before it
// are oversized and reserved for single-line mode, where one short line of
// digits can grow past the normal ~32px cap. time_fonts[4] is the 32px entry.
#define TIME_FONT_WRAP_START 4
static GFont time_fonts[TIME_FONT_COUNT];
static int time_layer_width;
static int layer_inset;

static Language current_language = LANG_NO;

#define TIME_BUFFER_SIZE 86
#define DATE_BUFFER_SIZE 20
#define PERSIST_KEY_LANGUAGE 1
#define PERSIST_KEY_BG_COLOR 3
#define PERSIST_KEY_TIME_COLOR 4
#define PERSIST_KEY_DATE_COLOR 5
#define PERSIST_KEY_TOP_LEFT 8
#define PERSIST_KEY_TOP_CENTER 9
#define PERSIST_KEY_TOP_RIGHT 10
#define PERSIST_KEY_SHOW_DATE 11
#define PERSIST_KEY_NUMERIC 12
#define PERSIST_KEY_ONELINE 13
#define PERSIST_KEY_HOUR24 14
#define PERSIST_KEY_TARGET_STEPS 15
#define PERSIST_KEY_TARGET_SLEEP 17
#define TIME_LAYER_HEIGHT 80
// Emery (Pebble Time 2, 200x228) has room for a taller top strip per the design
// research; other platforms keep the compact panels.
#if defined(PBL_PLATFORM_EMERY)
#define TOP_PANEL_HEIGHT 40
#define BOTTOM_PANEL_HEIGHT 32
#else
#define TOP_PANEL_HEIGHT 28
#define BOTTOM_PANEL_HEIGHT 28
#endif
#define METRIC_TEXT_SIZE 8
#define HR_SAMPLE_PERIOD_S 600
#define TOP_SLOT_COUNT 3

// Heart-rate zone thresholds (bpm). Below the first is "rest". The face colors
// the HR value by zone; see hr_zone_color().
#define HR_ZONE_FATBURN 100
#define HR_ZONE_CARDIO 140
#define HR_ZONE_PEAK 170

// Metric shown in a top slot. HR keeps value 1 so existing installs that
// persisted a heart-rate slot keep it after the upgrade.
typedef enum {
  SLOT_NONE = 0,
  SLOT_HR = 1,
  SLOT_STEPS = 2,
  SLOT_SLEEP = 3,
} SlotContent;

// Top row, left to right. Default: steps | (empty) | heart rate.
static SlotContent top_slots[TOP_SLOT_COUNT] = {SLOT_STEPS, SLOT_NONE, SLOT_HR};
static bool show_date = true;

// Daily goals: steps/sleep values turn green once met. Sleep stored in minutes.
static int target_steps = 10000;
static int target_sleep_min = 360;  // 6h

// Raw current readings, kept alongside the formatted buffers so color logic can
// compare against goals/zones.
static int steps_value = 0;
static int hr_value = 0;
static int sleep_value_min = 0;

static bool numeric_time = false;
static bool one_line_time = false;
static bool hour24_time = false;

static GColor bg_color;
static GColor time_color;
static GColor date_color;

static char time_buffer[TIME_BUFFER_SIZE];
static char date_buffer[DATE_BUFFER_SIZE];
static char hr_buffer[METRIC_TEXT_SIZE];
static char steps_buffer[METRIC_TEXT_SIZE];
static char sleep_buffer[METRIC_TEXT_SIZE];

// One panel layer for the top row (split into TOP_SLOT_COUNT cells) and one for
// the date at the bottom.
static Layer* top_panel = NULL;
static Layer* bottom_panel = NULL;
#if defined(PBL_HEALTH)
static bool health_active = false;
#endif

static bool slot_used(SlotContent metric) {
  for (int i = 0; i < TOP_SLOT_COUNT; i++) {
    if (top_slots[i] == metric) return true;
  }
  return false;
}

#if defined(PBL_HEALTH)
static bool health_shown(void) {
  return slot_used(SLOT_HR) || slot_used(SLOT_STEPS) || slot_used(SLOT_SLEEP);
}
#endif

static bool top_occupied(void) {
  for (int i = 0; i < TOP_SLOT_COUNT; i++) {
    if (top_slots[i] != SLOT_NONE) return true;
  }
  return false;
}

static bool bottom_occupied(void) { return show_date; }

static bool single_line(void) { return numeric_time && one_line_time; }

// Vertical room available to the time between the (optional) panels. Single-line
// mode sizes against this so a short line of digits can grow to fill the band
// instead of being pinned to the smaller wrapping-mode TIME_LAYER_HEIGHT.
static int time_band_height(void) {
  Layer* root = window_get_root_layer(window);
  int h = layer_get_bounds(root).size.h;
  int top = top_occupied() ? TOP_PANEL_HEIGHT : 0;
  int bottom = bottom_occupied() ? BOTTOM_PANEL_HEIGHT : 0;
  return h - top - bottom;
}

static GFont select_time_font(const char* text) {
  // Single-line mode measures with a wide unbounded width so the layout never
  // wraps, and against the full vertical band so the largest font that fits on
  // one line (by width and band height) is chosen.
  int max_h = single_line() ? time_band_height() : TIME_LAYER_HEIGHT;
  int start = single_line() ? 0 : TIME_FONT_WRAP_START;
  for (int i = start; i < TIME_FONT_COUNT; i++) {
    // Single-line: measure in an unbounded box (wide and tall) so the layout
    // never wraps or clips, then compare the true content size against the real
    // width/height limits. Wrapping mode measures inside the actual layer width.
    GRect box =
        single_line() ? GRect(0, 0, 2000, 2000) : GRect(0, 0, time_layer_width, TIME_LAYER_HEIGHT);
    GSize text_size = graphics_text_layout_get_content_size(
        text, time_fonts[i], box, GTextOverflowModeWordWrap, GTextAlignmentCenter);
    if (text_size.w <= time_layer_width && text_size.h <= max_h) {
      return time_fonts[i];
    }
  }
  return time_fonts[TIME_FONT_COUNT - 1];
}

// Heart drawn as two top lobes (circles) plus a triangle for the point.
static void draw_heart(GContext* ctx, GColor color, GPoint origin) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(origin.x + 3, origin.y + 3), 3);
  graphics_fill_circle(ctx, GPoint(origin.x + 9, origin.y + 3), 3);
  GPathInfo tri = {
      .num_points = 3,
      .points = (GPoint[]){
          {origin.x, origin.y + 4}, {origin.x + 12, origin.y + 4}, {origin.x + 6, origin.y + 12}}};
  GPath* path = gpath_create(&tri);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Footprint-style steps glyph: two small filled rounded blobs offset diagonally.
static void draw_steps(GContext* ctx, GColor color, GPoint origin) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(origin.x + 3, origin.y + 8), 3);
  graphics_fill_rect(ctx, GRect(origin.x, origin.y + 3, 6, 5), 2, GCornersTop);
  graphics_fill_circle(ctx, GPoint(origin.x + 9, origin.y + 4), 3);
  graphics_fill_rect(ctx, GRect(origin.x + 6, origin.y + 4, 6, 5), 2, GCornersBottom);
}

// Crescent moon for sleep: a filled disc with a background-colored disc punched
// out of its upper right to leave a crescent.
static void draw_moon(GContext* ctx, GColor color, GPoint origin) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(origin.x + 6, origin.y + 6), 6);
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_circle(ctx, GPoint(origin.x + 9, origin.y + 4), 5);
}

typedef void (*IconFn)(GContext*, GColor, GPoint);

static IconFn icon_for(SlotContent metric) {
  switch (metric) {
    case SLOT_HR:
      return draw_heart;
    case SLOT_STEPS:
      return draw_steps;
    case SLOT_SLEEP:
      return draw_moon;
    default:
      return NULL;
  }
}

static const char* metric_text(SlotContent metric) {
  switch (metric) {
    case SLOT_HR:
      return hr_buffer;
    case SLOT_STEPS:
      return steps_buffer;
    case SLOT_SLEEP:
      return sleep_buffer;
    default:
      return "";
  }
}

// Heart-rate zone color. On mono watches everything falls back to time_color.
static GColor hr_zone_color(int bpm) {
#if defined(PBL_COLOR)
  if (bpm <= 0) return time_color;               // no reading
  if (bpm < HR_ZONE_FATBURN) return time_color;  // rest: neutral
  if (bpm < HR_ZONE_CARDIO) return GColorGreen;
  if (bpm < HR_ZONE_PEAK) return GColorOrange;
  return GColorRed;
#else
  (void)bpm;
  return time_color;
#endif
}

// Color for a metric's icon + value: HR by zone, steps/sleep green when the goal
// is met, otherwise the configured time color.
static GColor metric_color(SlotContent metric) {
  switch (metric) {
    case SLOT_HR:
      return hr_zone_color(hr_value);
    case SLOT_STEPS:
      return PBL_IF_COLOR_ELSE(steps_value >= target_steps ? GColorGreen : time_color, time_color);
    case SLOT_SLEEP:
      return PBL_IF_COLOR_ELSE(sleep_value_min >= target_sleep_min ? GColorGreen : time_color,
                               time_color);
    default:
      return time_color;
  }
}

// Icon + value text for one metric, horizontally and vertically centered in
// cell. `icon` draws a 12x12 glyph at the given origin.
static void draw_metric_in_cell(GContext* ctx, GRect cell, const char* text, IconFn icon,
                                GColor color) {
  GSize text_size = graphics_text_layout_get_content_size(
      text, hr_font, cell, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  const int icon_w = 12;
  const int gap = 3;
  int content_w = icon_w + gap + text_size.w;
  int start_x = cell.origin.x + (cell.size.w - content_w) / 2;
  if (start_x < cell.origin.x) start_x = cell.origin.x;

  int icon_y = cell.origin.y + (cell.size.h - 12) / 2;
  icon(ctx, color, GPoint(start_x, icon_y));

  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(
      ctx, text, hr_font,
      GRect(start_x + icon_w + gap, cell.origin.y + (cell.size.h - text_size.h) / 2 - 2,
            cell.size.w - (start_x - cell.origin.x) - icon_w - gap, text_size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// Top row: split bounds into TOP_SLOT_COUNT equal cells, draw each slot's metric.
static void top_panel_update_proc(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cell_w = bounds.size.w / TOP_SLOT_COUNT;
  for (int i = 0; i < TOP_SLOT_COUNT; i++) {
    SlotContent metric = top_slots[i];
    IconFn icon = icon_for(metric);
    if (!icon) continue;
    GRect cell = GRect(bounds.origin.x + i * cell_w, bounds.origin.y, cell_w, bounds.size.h);
    draw_metric_in_cell(ctx, cell, metric_text(metric), icon, metric_color(metric));
  }
}

// Date text, horizontally and vertically centered in bounds.
static void bottom_panel_update_proc(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  GSize text_size = graphics_text_layout_get_content_size(
      date_buffer, date_font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);

  graphics_context_set_text_color(ctx, date_color);
  graphics_draw_text(ctx, date_buffer, date_font,
                     GRect(0, (bounds.size.h - text_size.h) / 2 - 2, bounds.size.w, text_size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Create the panel and attach `proc` if `wanted`, destroy it otherwise.
static void sync_panel(Layer** panel, bool wanted, LayerUpdateProc proc) {
  if (wanted && !*panel) {
    *panel = layer_create(GRect(0, 0, 0, 0));
    layer_set_update_proc(*panel, proc);
    layer_add_child(window_get_root_layer(window), *panel);
  } else if (!wanted && *panel) {
    layer_destroy(*panel);
    *panel = NULL;
  }
}

static void update_layout(GFont time_font) {
  Layer* root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  int center_top = top_occupied() ? TOP_PANEL_HEIGHT : 0;
  int center_bottom = bounds.size.h - (bottom_occupied() ? BOTTOM_PANEL_HEIGHT : 0);
  int center_height = center_bottom - center_top;

  // Single-line fonts can exceed TIME_LAYER_HEIGHT, so give the layer the full
  // band height to avoid clipping the taller glyphs.
  int layer_h = single_line() ? center_height : TIME_LAYER_HEIGHT;

  GSize time_size = graphics_text_layout_get_content_size(
      time_buffer, time_font, GRect(0, 0, time_layer_width, layer_h),
      single_line() ? GTextOverflowModeTrailingEllipsis : GTextOverflowModeWordWrap,
      GTextAlignmentCenter);

  int time_y = center_top + (center_height - time_size.h) / 2;

  layer_set_frame(text_layer_get_layer(time_layer),
                  GRect(layer_inset, time_y, time_layer_width, layer_h));

  if (top_panel) {
    layer_set_frame(top_panel, GRect(layer_inset, 0, time_layer_width, TOP_PANEL_HEIGHT));
  }
  if (bottom_panel) {
    layer_set_frame(bottom_panel, GRect(layer_inset, bounds.size.h - BOTTOM_PANEL_HEIGHT,
                                        time_layer_width, BOTTOM_PANEL_HEIGHT));
  }
}

static void mark_panels_dirty(void) {
  if (top_panel) layer_mark_dirty(top_panel);
  if (bottom_panel) layer_mark_dirty(bottom_panel);
}

// Reads the health metrics currently shown into their text buffers and marks
// the top panel for redraw. Off-watch (no health) all metrics read as "--".
static void refresh_metrics(void) {
#if defined(PBL_HEALTH)
  if (slot_used(SLOT_HR)) {
    HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
    hr_value = bpm > 0 ? (int)bpm : 0;
    if (hr_value > 0) {
      snprintf(hr_buffer, METRIC_TEXT_SIZE, "%d", hr_value);
    } else {
      snprintf(hr_buffer, METRIC_TEXT_SIZE, "--");
    }
  }

  if (slot_used(SLOT_STEPS)) {
    // Decimal thousands with unit, e.g. 8420 -> "8.4k". Under 1000 reads "0.xk".
    steps_value = (int)health_service_sum_today(HealthMetricStepCount);
    snprintf(steps_buffer, METRIC_TEXT_SIZE, "%d.%dk", steps_value / 1000,
             (steps_value % 1000) / 100);
  }

  if (slot_used(SLOT_SLEEP)) {
    sleep_value_min = (int)(health_service_sum_today(HealthMetricSleepSeconds) / 60);
    snprintf(sleep_buffer, METRIC_TEXT_SIZE, "%d:%02d", sleep_value_min / 60, sleep_value_min % 60);
  }
#endif
  if (top_panel) layer_mark_dirty(top_panel);
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void* context) {
  (void)context;
  if (event == HealthEventHeartRateUpdate || event == HealthEventMovementUpdate ||
      event == HealthEventSleepUpdate || event == HealthEventSignificantUpdate) {
    refresh_metrics();
  }
}

// Subscribes to health events and sets the heart-rate sampling period while any
// health metric is shown; unsubscribes when none are.
static void apply_health_state(void) {
  bool want = health_shown();

  if (want && !health_active) {
    health_service_events_subscribe(health_handler, NULL);
    health_active = true;
  } else if (!want && health_active) {
    health_service_events_unsubscribe();
    health_active = false;
  }

  if (want) {
    if (slot_used(SLOT_HR)) {
      health_service_set_heart_rate_sample_period(HR_SAMPLE_PERIOD_S);
    }
    refresh_metrics();
  }
}
#endif

// Recreates panel layers to match the current configuration and updates the
// health subscription accordingly.
static void apply_slots(void) {
  sync_panel(&top_panel, top_occupied(), top_panel_update_proc);
  sync_panel(&bottom_panel, show_date, bottom_panel_update_proc);

  if (!slot_used(SLOT_HR)) {
    snprintf(hr_buffer, METRIC_TEXT_SIZE, "--");
    hr_value = 0;
  }
  if (!slot_used(SLOT_STEPS)) {
    snprintf(steps_buffer, METRIC_TEXT_SIZE, "--");
    steps_value = 0;
  }
  if (!slot_used(SLOT_SLEEP)) {
    snprintf(sleep_buffer, METRIC_TEXT_SIZE, "--");
    sleep_value_min = 0;
  }

#if defined(PBL_HEALTH)
  apply_health_state();
#endif
}

static void refresh_clock(struct tm* time, TimeUnits units_changed) {
  (void)units_changed;
  date_to_words(current_language, time->tm_mday, time->tm_mon, time->tm_wday, date_buffer,
                DATE_BUFFER_SIZE);

  fuzzy_time_to_words(current_language, time->tm_hour, time->tm_min, numeric_time, hour24_time,
                      time_buffer, TIME_BUFFER_SIZE);
  GFont time_font = select_time_font(time_buffer);
  text_layer_set_overflow_mode(
      time_layer, single_line() ? GTextOverflowModeTrailingEllipsis : GTextOverflowModeWordWrap);
  text_layer_set_font(time_layer, time_font);
  text_layer_set_text(time_layer, time_buffer);

  update_layout(time_font);
  refresh_metrics();
  mark_panels_dirty();
}

static void force_refresh(void) {
  time_t t = time(NULL);
  struct tm* tm = localtime(&t);
  refresh_clock(tm, MINUTE_UNIT);
}

static void inbox_received_handler(DictionaryIterator* iter, void* context) {
  (void)context;
  bool changed = false;

  // Clay sends select items as cstrings (e.g. "1") and color items as int32.
  Tuple* lang_tuple = dict_find(iter, MESSAGE_KEY_Language);
  if (lang_tuple) {
    current_language = (Language)atoi(lang_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_LANGUAGE, current_language);
    changed = true;
  }

  Tuple* bg_tuple = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if (bg_tuple) {
    int bg_hex = bg_tuple->value->int32;
    persist_write_int(PERSIST_KEY_BG_COLOR, bg_hex);
    bg_color = GColorFromHEX(bg_hex);
    window_set_background_color(window, bg_color);
    changed = true;
  }

  Tuple* time_tuple = dict_find(iter, MESSAGE_KEY_TimeColor);
  if (time_tuple) {
    int time_hex = time_tuple->value->int32;
    persist_write_int(PERSIST_KEY_TIME_COLOR, time_hex);
    time_color = GColorFromHEX(time_hex);
    text_layer_set_text_color(time_layer, time_color);
    changed = true;
  }

  Tuple* date_tuple = dict_find(iter, MESSAGE_KEY_DateColor);
  if (date_tuple) {
    int date_hex = date_tuple->value->int32;
    persist_write_int(PERSIST_KEY_DATE_COLOR, date_hex);
    date_color = GColorFromHEX(date_hex);
    changed = true;
  }

  Tuple* top_left_tuple = dict_find(iter, MESSAGE_KEY_TopLeft);
  if (top_left_tuple) {
    top_slots[0] = (SlotContent)atoi(top_left_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_TOP_LEFT, top_slots[0]);
    changed = true;
  }

  Tuple* top_center_tuple = dict_find(iter, MESSAGE_KEY_TopCenter);
  if (top_center_tuple) {
    top_slots[1] = (SlotContent)atoi(top_center_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_TOP_CENTER, top_slots[1]);
    changed = true;
  }

  Tuple* top_right_tuple = dict_find(iter, MESSAGE_KEY_TopRight);
  if (top_right_tuple) {
    top_slots[2] = (SlotContent)atoi(top_right_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_TOP_RIGHT, top_slots[2]);
    changed = true;
  }

  Tuple* show_date_tuple = dict_find(iter, MESSAGE_KEY_ShowDate);
  if (show_date_tuple) {
    show_date = show_date_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_SHOW_DATE, show_date);
    changed = true;
  }

  Tuple* target_steps_tuple = dict_find(iter, MESSAGE_KEY_TargetSteps);
  if (target_steps_tuple) {
    target_steps = atoi(target_steps_tuple->value->cstring);
    if (target_steps < 1) target_steps = 1;
    persist_write_int(PERSIST_KEY_TARGET_STEPS, target_steps);
    changed = true;
  }

  Tuple* target_sleep_tuple = dict_find(iter, MESSAGE_KEY_TargetSleep);
  if (target_sleep_tuple) {
    // Sent as hours; stored as minutes.
    target_sleep_min = atoi(target_sleep_tuple->value->cstring) * 60;
    if (target_sleep_min < 1) target_sleep_min = 1;
    persist_write_int(PERSIST_KEY_TARGET_SLEEP, target_sleep_min);
    changed = true;
  }

  Tuple* numeric_tuple = dict_find(iter, MESSAGE_KEY_NumericTime);
  if (numeric_tuple) {
    numeric_time = numeric_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_NUMERIC, numeric_time);
    changed = true;
  }

  Tuple* oneline_tuple = dict_find(iter, MESSAGE_KEY_OneLineTime);
  if (oneline_tuple) {
    one_line_time = oneline_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_ONELINE, one_line_time);
    changed = true;
  }

  Tuple* hour24_tuple = dict_find(iter, MESSAGE_KEY_Hour24Time);
  if (hour24_tuple) {
    hour24_time = hour24_tuple->value->int32 != 0;
    persist_write_bool(PERSIST_KEY_HOUR24, hour24_time);
    changed = true;
  }

  apply_slots();
  force_refresh();

  if (changed) {
    vibes_short_pulse();
  }
}

static void setup_decorations(void) {
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, bg_color);

  Layer* root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  // Largest to smallest so select_time_font picks the first that fits on a line.
  // Rajdhani is a vector font, crisp at any size, so the ladder is a smooth
  // range. 56..36 are single-line only (see TIME_FONT_WRAP_START); normal
  // wrapping mode starts at 32.
  time_fonts[0] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_56));
  time_fonts[1] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_48));
  time_fonts[2] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_42));
  time_fonts[3] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_36));
  time_fonts[4] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_32));
  time_fonts[5] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_28));
  time_fonts[6] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_24));
  time_fonts[7] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_20));
  date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DATE_18));
  // Emery's taller top strip fits a larger value font; other platforms stay 18.
#if defined(PBL_PLATFORM_EMERY)
  hr_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#else
  hr_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#endif

#if PBL_ROUND
  layer_inset = 22;
#else
  layer_inset = 6;
#endif

  time_layer_width = bounds.size.w - (2 * layer_inset);
  time_layer = text_layer_create(GRect(layer_inset, 0, time_layer_width, TIME_LAYER_HEIGHT));
  text_layer_set_text_color(time_layer, time_color);
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_font(time_layer, time_fonts[0]);
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
  // The time phrase can span multiple words; wrap instead of clipping at the
  // edge so select_time_font's wrapped measurement matches what is drawn.
  text_layer_set_overflow_mode(time_layer, GTextOverflowModeWordWrap);
  layer_add_child(root, text_layer_get_layer(time_layer));

  snprintf(hr_buffer, METRIC_TEXT_SIZE, "--");
  snprintf(steps_buffer, METRIC_TEXT_SIZE, "--");
  snprintf(sleep_buffer, METRIC_TEXT_SIZE, "--");
  apply_slots();
}

static void load_settings(void) {
  bg_color = GColorBlack;
  time_color = GColorWhite;
  date_color = GColorLightGray;

  if (persist_exists(PERSIST_KEY_LANGUAGE)) {
    current_language = (Language)persist_read_int(PERSIST_KEY_LANGUAGE);
  }
  if (persist_exists(PERSIST_KEY_BG_COLOR)) {
    bg_color = GColorFromHEX(persist_read_int(PERSIST_KEY_BG_COLOR));
  }
  if (persist_exists(PERSIST_KEY_TIME_COLOR)) {
    time_color = GColorFromHEX(persist_read_int(PERSIST_KEY_TIME_COLOR));
  }
  if (persist_exists(PERSIST_KEY_DATE_COLOR)) {
    date_color = GColorFromHEX(persist_read_int(PERSIST_KEY_DATE_COLOR));
  }
  if (persist_exists(PERSIST_KEY_TOP_LEFT)) {
    top_slots[0] = (SlotContent)persist_read_int(PERSIST_KEY_TOP_LEFT);
  }
  if (persist_exists(PERSIST_KEY_TOP_CENTER)) {
    top_slots[1] = (SlotContent)persist_read_int(PERSIST_KEY_TOP_CENTER);
  }
  if (persist_exists(PERSIST_KEY_TOP_RIGHT)) {
    top_slots[2] = (SlotContent)persist_read_int(PERSIST_KEY_TOP_RIGHT);
  }
  if (persist_exists(PERSIST_KEY_SHOW_DATE)) {
    show_date = persist_read_bool(PERSIST_KEY_SHOW_DATE);
  }
  if (persist_exists(PERSIST_KEY_TARGET_STEPS)) {
    target_steps = persist_read_int(PERSIST_KEY_TARGET_STEPS);
  }
  if (persist_exists(PERSIST_KEY_TARGET_SLEEP)) {
    target_sleep_min = persist_read_int(PERSIST_KEY_TARGET_SLEEP);
  }
  if (persist_exists(PERSIST_KEY_NUMERIC)) {
    numeric_time = persist_read_bool(PERSIST_KEY_NUMERIC);
  }
  if (persist_exists(PERSIST_KEY_ONELINE)) {
    one_line_time = persist_read_bool(PERSIST_KEY_ONELINE);
  }
  if (persist_exists(PERSIST_KEY_HOUR24)) {
    hour24_time = persist_read_bool(PERSIST_KEY_HOUR24);
  }
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
#if defined(PBL_HEALTH)
  if (health_active) {
    health_service_events_unsubscribe();
  }
#endif
  if (top_panel) layer_destroy(top_panel);
  if (bottom_panel) layer_destroy(bottom_panel);
  text_layer_destroy(time_layer);
  for (int i = 0; i < TIME_FONT_COUNT; i++) {
    fonts_unload_custom_font(time_fonts[i]);
  }
  fonts_unload_custom_font(date_font);
  window_destroy(window);
}

int main(void) {
  load_settings();
  setup_decorations();

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);

  force_refresh();

  tick_timer_service_subscribe(MINUTE_UNIT, refresh_clock);

  app_event_loop();

  deinit();

  return 0;
}
