#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "num2words.h"


#define MY_UUID { 0xF3, 0xBF, 0xE6, 0x59, 0x51, 0xF8, 0x49, 0x9D, 0xAB, 0x25, 0x23, 0x9B, 0x4B, 0x8C, 0xB7, 0xF3 }
PBL_APP_INFO(MY_UUID,
             "Nortid", "Tindheim IT",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define BUFFER_SIZE 86

static struct CommonWordsData {
  TextLayer label;
  TextLayer date;
  Window window;
  char buffer[BUFFER_SIZE];
} s_data;

static void update_time(PblTm* t) {
  fuzzy_time_to_words(t->tm_hour, t->tm_min, s_data.buffer, BUFFER_SIZE);
  text_layer_set_text(&s_data.label, s_data.buffer);
}

static void update_date(PblTm* t){
  static char dateText[] = "Xxx 00 Xxx";

  PblTm currentTime;
  get_time(&currentTime);

  string_format_time(dateText, sizeof(dateText), "%a %e %b", &currentTime);
  text_layer_set_text(&s_data.date, dateText);
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
  GFont font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);

  text_layer_init(&s_data.label, GRect(0, 20, s_data.window.layer.frame.size.w, s_data.window.layer.frame.size.h - 20));
  text_layer_init(&s_data.date, GRect(20, 140, s_data.window.layer.frame.size.w, s_data.window.layer.frame.size.h - 0));
  text_layer_set_background_color(&s_data.label, GColorBlack);
  text_layer_set_background_color(&s_data.date, GColorBlack);
  text_layer_set_text_color(&s_data.label, GColorWhite);
  text_layer_set_text_color(&s_data.date, GColorWhite);
  text_layer_set_font(&s_data.label, font);
  text_layer_set_font(&s_data.date, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  layer_add_child(&s_data.window.layer, &s_data.label.layer);
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
