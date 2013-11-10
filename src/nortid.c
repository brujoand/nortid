#include <pebble.h>
#include "time2words.h"
#include "date2words.h"


char time_text[32];
char date_text[32];
TextLayer *time_layer;
TextLayer *date_layer;
Window *window;

static void update_time(struct tm *time, TimeUnits units_changed) {
  fuzzy_time_to_words(time->tm_hour, time->tm_min, time_text);
  text_layer_set_text(time_layer, time_text);
}

static void update_date(struct tm *time, TimeUnits units_changed){
  date_to_words(time->tm_mday, time->tm_mon, time->tm_wday, date_text);
  text_layer_set_text(date_layer, date_text);
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

  time_layer = add_text_layer(GRect(0, 20, 60, 22), time_font);
  time_layer = add_text_layer(GRect(0, 140, 120, 50), date_font);
}


int main() {
  setup_decorations();

  time_t t = time(0);
  struct tm *time = localtime(&t);
  update_time(time, -1);
  update_date(time, -1);

  tick_timer_service_subscribe(MINUTE_UNIT, update_time);
  tick_timer_service_subscribe(MINUTE_UNIT, update_date);

  app_event_loop();

  return 0;
}
