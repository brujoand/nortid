#include <pebble.h>
#include "time2words.h"
#include "date2words.h"

TextLayer *time_layer;
TextLayer *date_layer;
Window *window;

#define TIME_BUFFER_SIZE 86
#define DATE_BUFFER_SIZE 20

char time_buffer[TIME_BUFFER_SIZE];
char date_buffer[DATE_BUFFER_SIZE];

static void refresh_clock(struct tm *time, TimeUnits units_changed) {
  date_to_words(time->tm_mday, time->tm_mon, time->tm_wday, date_buffer, DATE_BUFFER_SIZE);
  text_layer_set_text(date_layer, date_buffer);

  fuzzy_time_to_words(time->tm_hour, time->tm_min, time_buffer, TIME_BUFFER_SIZE);
  text_layer_set_text(time_layer, time_buffer);
}

TextLayer *add_text_layer(GRect rect, GFont font) {
  TextLayer *layer = text_layer_create(rect);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_background_color(layer, GColorBlack);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
  return layer;
}

static void setup_decorations() {
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);

  GFont time_font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont date_font = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);

  time_layer = add_text_layer(GRect(0, 20, 140, 100), time_font);
  date_layer = add_text_layer(GRect(0, 140, 150, 50), date_font);
}


int main() {
  setup_decorations();

  time_t t = time(0);
  struct tm *time = localtime(&t);
  refresh_clock(time, -1);

  tick_timer_service_subscribe(MINUTE_UNIT, refresh_clock);

  app_event_loop();

  return 0;
}
