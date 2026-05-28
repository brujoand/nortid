#include <pebble.h>
#include <stdlib.h>

#include "date2words.h"
#include "lang/lang.h"
#include "time2words.h"

static TextLayer* time_layer;
static TextLayer* date_layer;
static Window* window;
static GFont date_font;

#define TIME_FONT_COUNT 3
static GFont time_fonts[TIME_FONT_COUNT];
static int time_layer_width;
static int layer_inset;

static Language current_language = LANG_NO;

#define TIME_BUFFER_SIZE 86
#define DATE_BUFFER_SIZE 20
#define PERSIST_KEY_LANGUAGE 1
#define PERSIST_KEY_SHOW_DATE 2
#define PERSIST_KEY_BG_COLOR 3
#define PERSIST_KEY_TIME_COLOR 4
#define PERSIST_KEY_DATE_COLOR 5
#define PERSIST_KEY_HR_PLACEMENT 6
#define PERSIST_KEY_HR_MODE 7
#define LAYER_SPACING 8
#define TIME_LAYER_HEIGHT 80
#define DATE_LAYER_HEIGHT 30
#define PANEL_HEIGHT 28

typedef enum { AREA_DISABLED = 0, AREA_HEADER = 1, AREA_FOOTER = 2 } ScreenArea;
typedef enum { HR_MODE_ALWAYS = 0, HR_MODE_AWAKE = 1 } HeartRateMode;

static bool show_date = true;
static AppTimer* hide_date_timer = NULL;

static ScreenArea hr_placement = AREA_DISABLED;
static HeartRateMode hr_mode = HR_MODE_ALWAYS;

static GColor bg_color;
static GColor time_color;
static GColor date_color;

static char time_buffer[TIME_BUFFER_SIZE];
static char date_buffer[DATE_BUFFER_SIZE];

#if defined(PBL_HEALTH)
#define HR_TEXT_SIZE 8
#define HR_SAMPLE_PERIOD_S 600
static Layer* hr_layer = NULL;
static GFont hr_font;
static char hr_buffer[HR_TEXT_SIZE];
static bool hr_active = false;
#endif

static bool date_visible(void) { return show_date || hide_date_timer != NULL; }

static bool header_occupied(void) { return hr_placement == AREA_HEADER; }

static bool footer_occupied(void) { return hr_placement == AREA_FOOTER; }

static GFont select_time_font(const char* text) {
  GSize max_size = GSize(time_layer_width, TIME_LAYER_HEIGHT);
  for (int i = 0; i < TIME_FONT_COUNT; i++) {
    GSize text_size = graphics_text_layout_get_content_size(
        text, time_fonts[i], GRect(0, 0, max_size.w, max_size.h), GTextOverflowModeWordWrap,
        GTextAlignmentCenter);
    if (text_size.w <= max_size.w) {
      return time_fonts[i];
    }
  }
  return time_fonts[TIME_FONT_COUNT - 1];
}

static void update_layout(GFont time_font) {
  Layer* root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  int center_top = header_occupied() ? PANEL_HEIGHT : 0;
  int center_bottom = bounds.size.h - (footer_occupied() ? PANEL_HEIGHT : 0);
  int center_height = center_bottom - center_top;

  GSize time_size = graphics_text_layout_get_content_size(
      time_buffer, time_font, GRect(0, 0, time_layer_width, TIME_LAYER_HEIGHT),
      GTextOverflowModeWordWrap, GTextAlignmentCenter);

  int total_height = time_size.h;
  if (date_visible()) {
    total_height += LAYER_SPACING + DATE_LAYER_HEIGHT;
  }

  int time_y = center_top + (center_height - total_height) / 2;
  int date_y = time_y + time_size.h + LAYER_SPACING;

  layer_set_frame(text_layer_get_layer(time_layer),
                  GRect(layer_inset, time_y, time_layer_width, TIME_LAYER_HEIGHT));
  layer_set_frame(text_layer_get_layer(date_layer),
                  GRect(layer_inset, date_y, time_layer_width, DATE_LAYER_HEIGHT));

#if defined(PBL_HEALTH)
  if (hr_layer) {
    int panel_y = header_occupied() ? 0 : (bounds.size.h - PANEL_HEIGHT);
    layer_set_frame(hr_layer, GRect(layer_inset, panel_y, time_layer_width, PANEL_HEIGHT));
  }
#endif
}

#if defined(PBL_HEALTH)
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

static void hr_layer_update_proc(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  GSize text_size = graphics_text_layout_get_content_size(
      hr_buffer, hr_font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  const int icon_w = 12;
  const int gap = 4;
  int content_w = icon_w + gap + text_size.w;
  int start_x = (bounds.size.w - content_w) / 2;
  if (start_x < 0) start_x = 0;

  int icon_y = (bounds.size.h - 12) / 2;
  draw_heart(ctx, time_color, GPoint(start_x, icon_y));

  graphics_context_set_text_color(ctx, time_color);
  graphics_draw_text(ctx, hr_buffer, hr_font,
                     GRect(start_x + icon_w + gap, (bounds.size.h - text_size.h) / 2 - 2,
                           bounds.size.w - start_x - icon_w - gap, text_size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void refresh_heart_rate(void) {
  if (!hr_layer) return;
  HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm > 0) {
    snprintf(hr_buffer, HR_TEXT_SIZE, "%d", (int)bpm);
  } else {
    snprintf(hr_buffer, HR_TEXT_SIZE, "--");
  }
  layer_mark_dirty(hr_layer);
}

static void health_handler(HealthEventType event, void* context) {
  (void)context;
  if (event == HealthEventHeartRateUpdate || event == HealthEventSignificantUpdate) {
    refresh_heart_rate();
  }
}

// Creates/destroys the HR layer and (un)subscribes to health events so they
// match the current placement and refresh-mode settings.
static void apply_hr_state(void) {
  bool want = (hr_placement != AREA_DISABLED);

  if (want && !hr_layer) {
    Layer* root = window_get_root_layer(window);
    hr_layer = layer_create(GRect(0, 0, 0, 0));
    layer_set_update_proc(hr_layer, hr_layer_update_proc);
    layer_add_child(root, hr_layer);
    snprintf(hr_buffer, HR_TEXT_SIZE, "--");
  } else if (!want && hr_layer) {
    layer_destroy(hr_layer);
    hr_layer = NULL;
  }

  if (want && !hr_active) {
    health_service_events_subscribe(health_handler, NULL);
    hr_active = true;
  } else if (!want && hr_active) {
    health_service_events_unsubscribe();
    hr_active = false;
  }

  if (want) {
    // Always-on keeps the sensor measuring in the background; awake mode lets
    // the system sample only while the watch is active (lower battery cost).
    health_service_set_heart_rate_sample_period(hr_mode == HR_MODE_ALWAYS ? HR_SAMPLE_PERIOD_S : 0);
    refresh_heart_rate();
  }
}
#endif

static void refresh_clock(struct tm* time, TimeUnits units_changed) {
  (void)units_changed;
  date_to_words(current_language, time->tm_mday, time->tm_mon, time->tm_wday, date_buffer,
                DATE_BUFFER_SIZE);
  text_layer_set_text(date_layer, date_buffer);

  fuzzy_time_to_words(current_language, time->tm_hour, time->tm_min, time_buffer, TIME_BUFFER_SIZE);
  GFont time_font = select_time_font(time_buffer);
  text_layer_set_font(time_layer, time_font);
  text_layer_set_text(time_layer, time_buffer);

  update_layout(time_font);

#if defined(PBL_HEALTH)
  refresh_heart_rate();
#endif
}

static void force_refresh(void) {
  time_t t = time(NULL);
  struct tm* tm = localtime(&t);
  refresh_clock(tm, MINUTE_UNIT);
}

static TextLayer* add_text_layer(GRect rect, GFont font, GColor text_clr) {
  TextLayer* layer = text_layer_create(rect);
  text_layer_set_text_color(layer, text_clr);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
  return layer;
}

static void hide_date_callback(void* data) {
  (void)data;
  hide_date_timer = NULL;
  layer_set_hidden(text_layer_get_layer(date_layer), true);
  force_refresh();
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  (void)axis;
  (void)direction;
  if (show_date) return;

  if (hide_date_timer) {
    app_timer_cancel(hide_date_timer);
  }
  layer_set_hidden(text_layer_get_layer(date_layer), false);
  force_refresh();
  hide_date_timer = app_timer_register(5000, hide_date_callback, NULL);
}

static void inbox_received_handler(DictionaryIterator* iter, void* context) {
  (void)context;
  bool changed = false;

  // Clay sends select and color items as cstrings (e.g. "1", "0xFFFFFF").
  Tuple* lang_tuple = dict_find(iter, MESSAGE_KEY_Language);
  if (lang_tuple) {
    current_language = (Language)atoi(lang_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_LANGUAGE, current_language);
    changed = true;
  }

  Tuple* show_date_tuple = dict_find(iter, MESSAGE_KEY_ShowDate);
  if (show_date_tuple) {
    show_date = (bool)show_date_tuple->value->int32;
    persist_write_bool(PERSIST_KEY_SHOW_DATE, show_date);
    layer_set_hidden(text_layer_get_layer(date_layer), !show_date);
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
    text_layer_set_text_color(date_layer, date_color);
    changed = true;
  }

  Tuple* hr_placement_tuple = dict_find(iter, MESSAGE_KEY_HeartRatePlacement);
  if (hr_placement_tuple) {
    hr_placement = (ScreenArea)atoi(hr_placement_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_HR_PLACEMENT, hr_placement);
    changed = true;
  }

  Tuple* hr_mode_tuple = dict_find(iter, MESSAGE_KEY_HeartRateMode);
  if (hr_mode_tuple) {
    hr_mode = (HeartRateMode)atoi(hr_mode_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_HR_MODE, hr_mode);
    changed = true;
  }

#if defined(PBL_HEALTH)
  apply_hr_state();
#endif

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

  time_fonts[0] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_28));
  time_fonts[1] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_24));
  time_fonts[2] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_20));
  date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_18));

#if defined(PBL_HEALTH)
  hr_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#endif

#if PBL_ROUND
  layer_inset = 22;
#else
  layer_inset = 6;
#endif

  time_layer_width = bounds.size.w - (2 * layer_inset);
  time_layer = add_text_layer(GRect(layer_inset, 0, time_layer_width, TIME_LAYER_HEIGHT),
                              time_fonts[0], time_color);
  date_layer = add_text_layer(GRect(layer_inset, 0, time_layer_width, DATE_LAYER_HEIGHT), date_font,
                              date_color);

  layer_set_hidden(text_layer_get_layer(date_layer), !show_date);

#if defined(PBL_HEALTH)
  apply_hr_state();
#endif
}

static void load_settings(void) {
  bg_color = GColorBlack;
  time_color = GColorWhite;
  date_color = GColorLightGray;

  if (persist_exists(PERSIST_KEY_LANGUAGE)) {
    current_language = (Language)persist_read_int(PERSIST_KEY_LANGUAGE);
  }
  if (persist_exists(PERSIST_KEY_SHOW_DATE)) {
    show_date = persist_read_bool(PERSIST_KEY_SHOW_DATE);
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
  if (persist_exists(PERSIST_KEY_HR_PLACEMENT)) {
    hr_placement = (ScreenArea)persist_read_int(PERSIST_KEY_HR_PLACEMENT);
  }
  if (persist_exists(PERSIST_KEY_HR_MODE)) {
    hr_mode = (HeartRateMode)persist_read_int(PERSIST_KEY_HR_MODE);
  }
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  if (hide_date_timer) {
    app_timer_cancel(hide_date_timer);
  }
#if defined(PBL_HEALTH)
  if (hr_active) {
    health_service_events_unsubscribe();
  }
  if (hr_layer) {
    layer_destroy(hr_layer);
  }
#endif
  text_layer_destroy(time_layer);
  text_layer_destroy(date_layer);
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
  accel_tap_service_subscribe(tap_handler);

  app_event_loop();

  deinit();

  return 0;
}
