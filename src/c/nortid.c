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
#define PERSIST_KEY_TOP_SLOT 8
#define PERSIST_KEY_BOTTOM_SLOT 10
#define PERSIST_KEY_NUMERIC 12
#define PERSIST_KEY_ONELINE 13
#define PERSIST_KEY_HOUR24 14
#define TIME_LAYER_HEIGHT 80
#define PANEL_HEIGHT 28
#define HR_TEXT_SIZE 8
#define HR_SAMPLE_PERIOD_S 600

typedef enum { SLOT_NONE = 0, SLOT_HR = 1, SLOT_DATE = 2 } SlotContent;

static SlotContent top_slot = SLOT_HR;
static SlotContent bottom_slot = SLOT_DATE;

static bool numeric_time = false;
static bool one_line_time = false;
static bool hour24_time = false;

static GColor bg_color;
static GColor time_color;
static GColor date_color;

static char time_buffer[TIME_BUFFER_SIZE];
static char date_buffer[DATE_BUFFER_SIZE];
static char hr_buffer[HR_TEXT_SIZE];

// Generic panel layers. Each holds one SlotContent (passed via layer data) so a
// single update proc can render either heart rate or the date in either panel.
static Layer* top_panel = NULL;
static Layer* bottom_panel = NULL;
#if defined(PBL_HEALTH)
static bool health_active = false;
#endif

static bool hr_shown(void) { return top_slot == SLOT_HR || bottom_slot == SLOT_HR; }

static bool top_occupied(void) { return top_slot != SLOT_NONE; }

static bool bottom_occupied(void) { return bottom_slot != SLOT_NONE; }

static bool single_line(void) { return numeric_time && one_line_time; }

// Vertical room available to the time between the (optional) panels. Single-line
// mode sizes against this so a short line of digits can grow to fill the band
// instead of being pinned to the smaller wrapping-mode TIME_LAYER_HEIGHT.
static int time_band_height(void) {
  Layer* root = window_get_root_layer(window);
  int h = layer_get_bounds(root).size.h;
  int top = top_occupied() ? PANEL_HEIGHT : 0;
  int bottom = bottom_occupied() ? PANEL_HEIGHT : 0;
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

// Heart icon + bpm text, horizontally and vertically centered in bounds.
static void draw_hr_in_rect(GContext* ctx, GRect bounds) {
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

// Date text, horizontally and vertically centered in bounds.
static void draw_date_in_rect(GContext* ctx, GRect bounds) {
  GSize text_size = graphics_text_layout_get_content_size(
      date_buffer, date_font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);

  graphics_context_set_text_color(ctx, date_color);
  graphics_draw_text(ctx, date_buffer, date_font,
                     GRect(0, (bounds.size.h - text_size.h) / 2 - 2, bounds.size.w, text_size.h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void panel_update_proc(Layer* layer, GContext* ctx) {
  SlotContent content = *(SlotContent*)layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);
  if (content == SLOT_HR) {
    draw_hr_in_rect(ctx, bounds);
  } else if (content == SLOT_DATE) {
    draw_date_in_rect(ctx, bounds);
  }
}

static Layer* create_panel(SlotContent content) {
  Layer* layer = layer_create_with_data(GRect(0, 0, 0, 0), sizeof(SlotContent));
  *(SlotContent*)layer_get_data(layer) = content;
  layer_set_update_proc(layer, panel_update_proc);
  layer_add_child(window_get_root_layer(window), layer);
  return layer;
}

// Create the panel for `slot` if it holds content, destroy it otherwise. Keeps
// the stored content marker in sync with the current slot value.
static void sync_panel(Layer** panel, SlotContent slot) {
  if (slot != SLOT_NONE && !*panel) {
    *panel = create_panel(slot);
  } else if (slot == SLOT_NONE && *panel) {
    layer_destroy(*panel);
    *panel = NULL;
  } else if (*panel) {
    *(SlotContent*)layer_get_data(*panel) = slot;
  }
}

static void update_layout(GFont time_font) {
  Layer* root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  int center_top = top_occupied() ? PANEL_HEIGHT : 0;
  int center_bottom = bounds.size.h - (bottom_occupied() ? PANEL_HEIGHT : 0);
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
    layer_set_frame(top_panel, GRect(layer_inset, 0, time_layer_width, PANEL_HEIGHT));
  }
  if (bottom_panel) {
    layer_set_frame(bottom_panel, GRect(layer_inset, bounds.size.h - PANEL_HEIGHT, time_layer_width,
                                        PANEL_HEIGHT));
  }
}

static void mark_panels_dirty(void) {
  if (top_panel) layer_mark_dirty(top_panel);
  if (bottom_panel) layer_mark_dirty(bottom_panel);
}

static void refresh_heart_rate(void) {
#if defined(PBL_HEALTH)
  HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm > 0) {
    snprintf(hr_buffer, HR_TEXT_SIZE, "%d", (int)bpm);
  } else {
    snprintf(hr_buffer, HR_TEXT_SIZE, "--");
  }
#endif
  mark_panels_dirty();
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void* context) {
  (void)context;
  if (event == HealthEventHeartRateUpdate || event == HealthEventSignificantUpdate) {
    refresh_heart_rate();
  }
}

// Subscribes to health events and sets the sampling period while heart rate is
// shown in any slot; unsubscribes when it is not.
static void apply_hr_state(void) {
  bool want = hr_shown();

  if (want && !health_active) {
    health_service_events_subscribe(health_handler, NULL);
    health_active = true;
  } else if (!want && health_active) {
    health_service_events_unsubscribe();
    health_active = false;
  }

  if (want) {
    health_service_set_heart_rate_sample_period(HR_SAMPLE_PERIOD_S);
    refresh_heart_rate();
  }
}
#endif

// Recreates panel layers to match the current slot configuration and updates
// the health subscription accordingly.
static void apply_slots(void) {
  sync_panel(&top_panel, top_slot);
  sync_panel(&bottom_panel, bottom_slot);

  if (!hr_shown()) {
    snprintf(hr_buffer, HR_TEXT_SIZE, "--");
  }

#if defined(PBL_HEALTH)
  apply_hr_state();
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
  refresh_heart_rate();
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

  Tuple* top_tuple = dict_find(iter, MESSAGE_KEY_TopSlot);
  if (top_tuple) {
    top_slot = (SlotContent)atoi(top_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_TOP_SLOT, top_slot);
    changed = true;
  }

  Tuple* bottom_tuple = dict_find(iter, MESSAGE_KEY_BottomSlot);
  if (bottom_tuple) {
    bottom_slot = (SlotContent)atoi(bottom_tuple->value->cstring);
    persist_write_int(PERSIST_KEY_BOTTOM_SLOT, bottom_slot);
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
  hr_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

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

  snprintf(hr_buffer, HR_TEXT_SIZE, "--");
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
  if (persist_exists(PERSIST_KEY_TOP_SLOT)) {
    top_slot = (SlotContent)persist_read_int(PERSIST_KEY_TOP_SLOT);
  }
  if (persist_exists(PERSIST_KEY_BOTTOM_SLOT)) {
    bottom_slot = (SlotContent)persist_read_int(PERSIST_KEY_BOTTOM_SLOT);
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
