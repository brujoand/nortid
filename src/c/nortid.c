#include <pebble.h>

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

static Language current_language = LANG_NO;

#define TIME_BUFFER_SIZE 86
#define DATE_BUFFER_SIZE 20
#define PERSIST_KEY_LANGUAGE 1
#define PERSIST_KEY_SHOW_DATE 2

static bool show_date = true;

static char time_buffer[TIME_BUFFER_SIZE];
static char date_buffer[DATE_BUFFER_SIZE];

static GFont select_time_font(const char* text) {
  GSize max_size = GSize(time_layer_width, 80);
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

static void refresh_clock(struct tm* time, TimeUnits units_changed) {
  (void)units_changed;
  date_to_words(current_language, time->tm_mday, time->tm_mon, time->tm_wday, date_buffer,
                DATE_BUFFER_SIZE);
  text_layer_set_text(date_layer, date_buffer);

  fuzzy_time_to_words(current_language, time->tm_hour, time->tm_min, time_buffer, TIME_BUFFER_SIZE);
  text_layer_set_font(time_layer, select_time_font(time_buffer));
  text_layer_set_text(time_layer, time_buffer);
}

static void force_refresh(void) {
  time_t t = time(NULL);
  struct tm* tm = localtime(&t);
  refresh_clock(tm, MINUTE_UNIT);
}

static TextLayer* add_text_layer(GRect rect, GFont font) {
  TextLayer* layer = text_layer_create(rect);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_background_color(layer, GColorBlack);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
  return layer;
}

static void inbox_received_handler(DictionaryIterator* iter, void* context) {
  (void)context;
  Tuple* lang_tuple = dict_find(iter, MESSAGE_KEY_Language);
  if (lang_tuple) {
    current_language = (Language)lang_tuple->value->int32;
    persist_write_int(PERSIST_KEY_LANGUAGE, current_language);
  }

  Tuple* show_date_tuple = dict_find(iter, MESSAGE_KEY_ShowDate);
  if (show_date_tuple) {
    show_date = (bool)show_date_tuple->value->int32;
    persist_write_bool(PERSIST_KEY_SHOW_DATE, show_date);
    layer_set_hidden(text_layer_get_layer(date_layer), !show_date);
  }

  force_refresh();
}

static void setup_decorations(void) {
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);

  Layer* root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  time_fonts[0] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_28));
  time_fonts[1] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_24));
  time_fonts[2] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_20));
  date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PLEX_BOLD_18));

#if PBL_ROUND
  int inset = 22;
  int time_y = 40;
  int date_y = bounds.size.h - 60;
#else
  int inset = 6;
  int time_y = 25;
  int date_y = bounds.size.h - 45;
#endif

  time_layer_width = bounds.size.w - (2 * inset);
  time_layer = add_text_layer(GRect(inset, time_y, time_layer_width, 80), time_fonts[0]);
  date_layer = add_text_layer(GRect(inset, date_y, bounds.size.w - (2 * inset), 30), date_font);
  layer_set_hidden(text_layer_get_layer(date_layer), !show_date);
}

static void load_settings(void) {
  if (persist_exists(PERSIST_KEY_LANGUAGE)) {
    current_language = (Language)persist_read_int(PERSIST_KEY_LANGUAGE);
  }
  if (persist_exists(PERSIST_KEY_SHOW_DATE)) {
    show_date = persist_read_bool(PERSIST_KEY_SHOW_DATE);
  }
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
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
  app_message_open(64, 64);

  force_refresh();

  tick_timer_service_subscribe(MINUTE_UNIT, refresh_clock);

  app_event_loop();

  deinit();

  return 0;
}
