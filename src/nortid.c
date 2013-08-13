#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "time2words.h"
#include "date2words.h"


#define MY_UUID { 0xF3, 0xBF, 0xE6, 0x59, 0x51, 0xF8, 0x49, 0x9D, 0xAB, 0x25, 0x23, 0x9B, 0x4B, 0x8C, 0xB7, 0xF3 }
PBL_APP_INFO(MY_UUID,
             "Nortid", "Tindheim IT",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define TIME_BUFFER_SIZE 86
#define DATE_BUFFER_SIZE 20

static struct CommonWordsData {
  TextLayer time;
  TextLayer date;
  Window window;
  char timeBuffer[TIME_BUFFER_SIZE];
  char dateBuffer[DATE_BUFFER_SIZE];
} s_data;

static void update_time(PblTm* t) {
  fuzzy_time_to_words(t->tm_hour, t->tm_min, s_data.timeBuffer, TIME_BUFFER_SIZE);
  text_layer_set_text(&s_data.time, s_data.timeBuffer);
}

static void update_date(PblTm* t){
  date_to_words(t->tm_mday, t->tm_mon, t->tm_wday, s_data.dateBuffer, DATE_BUFFER_SIZE);
  text_layer_set_text(&s_data.date, s_data.dateBuffer);
}

static void handle_minute_tick(AppContextRef app_ctx, PebbleTickEvent* e) {
  update_time(e->tick_time);
  update_date(e->tick_time);
}


static void handle_init(AppContextRef ctx) {

  window_init(&s_data.window, "Norwegian time");
  const bool animated = true;

  window_stack_push(&s_data.window, animated);
  window_set_background_color(&s_data.window, GColorBlack);

  text_layer_init(&s_data.time, GRect(0, 20, s_data.window.layer.frame.size.w, s_data.window.layer.frame.size.h - 20));  
  text_layer_set_background_color(&s_data.time, GColorBlack);  
  text_layer_set_text_color(&s_data.time, GColorWhite);
  text_layer_set_font(&s_data.time, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(&s_data.time, GTextAlignmentCenter);
  layer_add_child(&s_data.window.layer, &s_data.time.layer);

  text_layer_init(&s_data.date, GRect(0, 140, s_data.window.layer.frame.size.w, s_data.window.layer.frame.size.h));
  text_layer_set_background_color(&s_data.date, GColorBlack);
  text_layer_set_text_color(&s_data.date, GColorWhite);
  text_layer_set_font(&s_data.date, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_alignment(&s_data.date, GTextAlignmentCenter);
  layer_add_child(&s_data.window.layer, &s_data.date.layer);

  PblTm t;
  get_time(&t);
  update_time(&t);
  update_date(&t);
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
