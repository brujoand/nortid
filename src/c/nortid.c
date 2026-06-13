#include <pebble.h>
#include <stdlib.h>

#include "date2words.h"
#include "lang/lang.h"
#include "time2words.h"

static TextLayer* time_layer;
static Window* window;
static GFont date_font;
static GFont hr_font;

#define TIME_FONT_COUNT 9
// The whole ladder is usable in every mode. The worded fitter chooses a line
// count first (1 line if the phrase fits full width, else a balanced 2 lines,
// else 3) and then takes the largest font that fills that layout.
static GFont time_fonts[TIME_FONT_COUNT];
// Minimum side margin: the longest line is allowed to grow until it is within
// this many pixels of the layer width.
#define TIME_SIDE_MARGIN 2
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
#define TIME_LAYER_HEIGHT 80
// Emery (Pebble Time 2, 200x228) has room for a taller top strip per the design
// research; other platforms keep the compact panels.
//
// The top panel stacks a label above the value inside a solid inverse tile, so
// it is taller than the bottom (date-only) panel. The extra height comes out of
// the time band, which the worded fitter absorbs by breaking to two lines.
#if defined(PBL_PLATFORM_EMERY)
#define TOP_PANEL_HEIGHT 44
#define BOTTOM_PANEL_HEIGHT 32
#else
#define TOP_PANEL_HEIGHT 40
#define BOTTOM_PANEL_HEIGHT 32
#endif
#define METRIC_TEXT_SIZE 8
#define HR_SAMPLE_PERIOD_S 600
#define TOP_SLOT_COUNT 3

// Metric shown in a top slot. HR keeps value 1 so existing installs that
// persisted a heart-rate slot keep it after the upgrade.
typedef enum {
  SLOT_NONE = 0,
  SLOT_HR = 1,
  SLOT_STEPS = 2,
  SLOT_SLEEP = 3,
} SlotContent;

// Top row, left to right. Default: sleep | heart rate | steps.
static SlotContent top_slots[TOP_SLOT_COUNT] = {SLOT_SLEEP, SLOT_HR, SLOT_STEPS};
static bool show_date = true;

// Raw current readings, used to format the displayed value buffers.
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

// Measures one candidate string at a given font in a width-unbounded box so the
// layout breaks only on the explicit '\n' we inserted, never on width. The
// returned width is then the true widest line and the height the true stack
// height; a layout fits when both are within the usable box. Measuring in a
// max_w-wide box instead would let an over-wide line silently wrap and report a
// deceptively small width.
static bool layout_fits(const char* text, GFont font, int max_w, int max_h) {
  GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 2000, 2000), GTextOverflowModeWordWrap, GTextAlignmentCenter);
  return size.w <= max_w && size.h <= max_h;
}

// Largest font in the ladder for which `text` (as broken) fits, or -1 if none.
// Searches largest-first and returns the first that fits.
static int largest_fitting_font(const char* text, int max_w, int max_h) {
  for (int i = 0; i < TIME_FONT_COUNT; i++) {
    if (layout_fits(text, time_fonts[i], max_w, max_h)) return i;
  }
  return -1;
}

// Writes `src` into `dst` with the space before word index `break_at` replaced
// by a newline, splitting the phrase into two lines. `break_at` is 1-based in
// word count (1 => break before the 2nd word).
static void split_at_word(const char* src, int break_at, char* dst, size_t dst_len) {
  size_t di = 0;
  int word = 0;
  for (size_t si = 0; src[si] != '\0' && di + 1 < dst_len; si++) {
    if (src[si] == ' ') {
      word++;
      dst[di++] = (word == break_at) ? '\n' : ' ';
    } else {
      dst[di++] = src[si];
    }
  }
  dst[di] = '\0';
}

// Picks the balanced two-line split: the break minimizing the wider line's
// width, measured at the largest font. That split leaves the most room to grow.
// Returns the 1-based break word index, or 0 if the phrase has < 2 words.
static int balanced_break(const char* text) {
  int spaces = 0;
  for (const char* p = text; *p; p++) {
    if (*p == ' ') spaces++;
  }
  if (spaces < 1) return 0;

  GFont ref = time_fonts[0];
  int best_break = 1;
  int best_max_line = 100000;
  char candidate[TIME_BUFFER_SIZE];
  for (int b = 1; b <= spaces; b++) {
    split_at_word(text, b, candidate, sizeof(candidate));
    // Width-unbounded measure so size.w is the true widest of the two lines
    // (the '\n' is the only break); we want the split that minimizes it.
    GSize size = graphics_text_layout_get_content_size(
        candidate, ref, GRect(0, 0, 2000, 2000), GTextOverflowModeWordWrap, GTextAlignmentLeft);
    if (size.w < best_max_line) {
      best_max_line = size.w;
      best_break = b;
    }
  }
  return best_break;
}

// Worded / numeric-wrapping fit. Maximizes the font size by comparing layouts:
// it measures the largest font that fits the phrase on one line and the largest
// that fits a balanced two-line split, then keeps whichever yields the bigger
// font (the ladder is largest-first, so a smaller index is a bigger font). This
// breaks a phrase onto two lines whenever that lets it grow, instead of keeping
// it on one small line. Ties prefer one line. Falls back to natural word-wrap at
// the smallest font when nothing fits. Writes the chosen string into out.
static GFont fit_worded_time(const char* text, char* out, size_t out_len) {
  int max_w = time_layer_width - TIME_SIDE_MARGIN;
  int max_h = time_band_height();

  char one_line[TIME_BUFFER_SIZE];
  strncpy(one_line, text, sizeof(one_line) - 1);
  one_line[sizeof(one_line) - 1] = '\0';
  int one_idx = largest_fitting_font(one_line, max_w, max_h);

  char two_line[TIME_BUFFER_SIZE];
  int two_idx = -1;
  int brk = balanced_break(text);
  if (brk > 0) {
    split_at_word(text, brk, two_line, sizeof(two_line));
    two_idx = largest_fitting_font(two_line, max_w, max_h);
  }

  // Pick the layout whose largest fitting font is biggest (smallest index).
  // The two-line split must be strictly bigger to be chosen, so equal-size
  // results keep the single line.
  if (two_idx >= 0 && (one_idx < 0 || two_idx < one_idx)) {
    strncpy(out, two_line, out_len - 1);
    out[out_len - 1] = '\0';
    return time_fonts[two_idx];
  }
  if (one_idx >= 0) {
    strncpy(out, one_line, out_len - 1);
    out[out_len - 1] = '\0';
    return time_fonts[one_idx];
  }

  // Nothing fit either way: natural word-wrap at the smallest font.
  strncpy(out, text, out_len - 1);
  out[out_len - 1] = '\0';
  return time_fonts[TIME_FONT_COUNT - 1];
}

// Single-line numeric fit: measure in an unbounded box so the layout never
// wraps, take the largest font whose digits fit the width and vertical band.
static GFont fit_single_line(const char* text) {
  int max_w = time_layer_width - TIME_SIDE_MARGIN;
  int max_h = time_band_height();
  for (int i = 0; i < TIME_FONT_COUNT; i++) {
    GSize size =
        graphics_text_layout_get_content_size(text, time_fonts[i], GRect(0, 0, 2000, 2000),
                                              GTextOverflowModeWordWrap, GTextAlignmentCenter);
    if (size.w <= max_w && size.h <= max_h) return time_fonts[i];
  }
  return time_fonts[TIME_FONT_COUNT - 1];
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

// Icon row height reserved above the value inside each tile.
#define TILE_ICON_H 12

// Heart: two top lobes with a visible cleft between them, tapering to a tip.
// Lobe centers are spread far enough apart that the gap reads at 12px, and the
// body is a triangle whose top row bridges the lobes so the silhouette closes.
static void draw_heart_icon(GContext* ctx, int cx, int cy, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  int r = 3;
  int lobe_y = cy - 2;
  // Spread the lobes so the cleft between them is one pixel wide, not merged.
  graphics_fill_circle(ctx, GPoint(cx - r + 1, lobe_y), r);
  graphics_fill_circle(ctx, GPoint(cx + r - 1, lobe_y), r);
  // Body: a tapering triangle from the lobes' full width down to the tip. Start
  // at the lobe centers' row so it fuses with the circles into one shape.
  int top_half = r + 1;
  for (int dy = 0; dy <= top_half + 1; dy++) {
    int half = top_half - dy;
    if (half < 0) half = 0;
    int y = lobe_y + dy + 1;
    graphics_draw_line(ctx, GPoint(cx - half, y), GPoint(cx + half, y));
  }
}

// Sleep: two bold "Z" glyphs (small over large) drawn with doubled strokes so
// they stay legible at 12px. Each Z is a top bar, a diagonal, and a bottom bar.
static void draw_z(GContext* ctx, int x, int y, int s) {
  for (int o = 0; o <= 1; o++) {  // thicken by drawing each stroke twice
    graphics_draw_line(ctx, GPoint(x, y + o), GPoint(x + s, y + o));          // top
    graphics_draw_line(ctx, GPoint(x + s - o, y), GPoint(x - o, y + s));      // diagonal
    graphics_draw_line(ctx, GPoint(x, y + s - o), GPoint(x + s, y + s - o));  // bottom
  }
}

static void draw_sleep_icon(GContext* ctx, int cx, int cy, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  // Large Z down-left, small Z up-right: the classic rising "zzz" reading.
  draw_z(ctx, cx - 7, cy - 1, 6);
  draw_z(ctx, cx + 1, cy - 6, 5);
}

// Steps: two footprints. Each print is a sole (a short vertical stack of
// widening then narrowing dots) plus a toe dot offset toward the lead, so it
// reads as a foot rather than scattered points. Left foot trails, right leads.
static void draw_foot(GContext* ctx, int fx, int fy) {
  graphics_fill_circle(ctx, GPoint(fx, fy), 2);          // sole (heel/arch)
  graphics_fill_circle(ctx, GPoint(fx, fy - 3), 2);      // sole (ball)
  graphics_fill_circle(ctx, GPoint(fx + 1, fy - 5), 1);  // toe
}

static void draw_steps_icon(GContext* ctx, int cx, int cy, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  // Left foot trails (lower), right foot leads (higher): reads as a stride.
  draw_foot(ctx, cx - 4, cy + 3);
  draw_foot(ctx, cx + 4, cy + 1);
}

// Draws the metric's icon centered in a band of TILE_ICON_H height whose top is
// at `top`, spanning the chip width.
static void draw_metric_icon(GContext* ctx, GRect chip, int top, SlotContent metric, GColor color) {
  int cx = chip.origin.x + chip.size.w / 2;
  int cy = top + TILE_ICON_H / 2;
  switch (metric) {
    case SLOT_HR:
      draw_heart_icon(ctx, cx, cy, color);
      break;
    case SLOT_STEPS:
      draw_steps_icon(ctx, cx, cy, color);
      break;
    case SLOT_SLEEP:
      draw_sleep_icon(ctx, cx, cy, color);
      break;
    default:
      break;
  }
}

// Tile chrome: rounded-rect border inset from the cell edges, and the gap
// between the label and value rows.
#define TILE_BORDER_INSET 2
#define TILE_BORDER_RADIUS 4
#define TILE_LABEL_GAP 0

// Draws a metric tile as a solid inverse chip: a filled rounded-rect in the
// foreground color (time_color) with the label stacked above the value inside
// it, both drawn in the background color so they read as knocked out. The chip's
// edge is its own fill, not a separate stroke.
static void draw_metric_in_cell(GContext* ctx, GRect cell, SlotContent metric, GColor fill,
                                GColor text) {
  const char* value = metric_text(metric);

  GRect chip = grect_inset(cell, GEdgeInsets(TILE_BORDER_INSET));
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, chip, TILE_BORDER_RADIUS, GCornersAll);

  GSize value_size = graphics_text_layout_get_content_size(
      value, hr_font, chip, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);

  // System font reports a tall line box; trim so the icon+value pair sits
  // optically centered in the chip rather than low.
  int value_h = value_size.h - 6;
  int group_h = TILE_ICON_H + TILE_LABEL_GAP + value_h;
  int top = chip.origin.y + (chip.size.h - group_h) / 2;

  draw_metric_icon(ctx, chip, top, metric, text);

  graphics_context_set_text_color(ctx, text);
  graphics_draw_text(
      ctx, value, hr_font,
      GRect(chip.origin.x, top + TILE_ICON_H + TILE_LABEL_GAP - 4, chip.size.w, value_size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// Top row: split bounds into TOP_SLOT_COUNT equal cells, each a bordered tile
// with a label above its value. Round's top strip is a narrow chord, so only the
// center slot renders there.
static void top_panel_update_proc(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cell_w = bounds.size.w / TOP_SLOT_COUNT;
  for (int i = 0; i < TOP_SLOT_COUNT; i++) {
    SlotContent metric = top_slots[i];
    if (metric == SLOT_NONE) continue;
#if PBL_ROUND
    if (i != 1) continue;
#endif
    GRect cell = GRect(bounds.origin.x + i * cell_w, bounds.origin.y, cell_w, bounds.size.h);
    // Inverse chip: foreground fill, background-colored text knocked out of it.
    draw_metric_in_cell(ctx, cell, metric, time_color, bg_color);
  }
}

// Width of the hairline drawn above the date, as a fraction of panel width.
#define DATE_RULE_WIDTH_NUM 1
#define DATE_RULE_WIDTH_DEN 3

// Date text, horizontally and vertically centered in bounds, with a short
// hairline rule centered above it to anchor the otherwise-floating date.
static void bottom_panel_update_proc(Layer* layer, GContext* ctx) {
  GRect bounds = layer_get_bounds(layer);
  GSize text_size = graphics_text_layout_get_content_size(
      date_buffer, date_font, bounds, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);

  int text_y = (bounds.size.h - text_size.h) / 2 - 2;

  // Hairline near the panel's top edge, spanning a centered fraction of the
  // width. Anchored to a fixed offset from the top rather than to text_y: the
  // 24px font's line box runs tall, so text_y lands near (and sometimes above)
  // y=0, which would clip a rule placed relative to it. date_color is the muted
  // accent already used for the date, so the rule reads as the same element on
  // every platform.
  int rule_w = bounds.size.w * DATE_RULE_WIDTH_NUM / DATE_RULE_WIDTH_DEN;
  int rule_x0 = (bounds.size.w - rule_w) / 2;
  int rule_y = 2;
  graphics_context_set_stroke_color(ctx, date_color);
  graphics_draw_line(ctx, GPoint(rule_x0, rule_y), GPoint(rule_x0 + rule_w, rule_y));

  graphics_context_set_text_color(ctx, date_color);
  graphics_draw_text(ctx, date_buffer, date_font, GRect(0, text_y, bounds.size.w, text_size.h),
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

  // Both modes can now pick fonts that fill the whole band, so give the layer
  // the full center height and center the measured text within it.
  int layer_h = center_height;

  GSize time_size = graphics_text_layout_get_content_size(
      time_buffer, time_font, GRect(0, 0, time_layer_width, layer_h),
      single_line() ? GTextOverflowModeTrailingEllipsis : GTextOverflowModeWordWrap,
      GTextAlignmentCenter);

  int time_y = center_top + (center_height - time_size.h) / 2;
  // The date panel now carries a hairline along its top edge, which pushes the
  // date down within its band. Lift the time block 4px to rebalance the gap
  // above and below it. Only when the date (hence the hairline) is shown.
  if (bottom_occupied()) {
    time_y -= 8;
  }

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

#ifdef SCREENSHOT_DEMO
  // Representative readings so appstore screenshots show real-looking metrics
  // instead of "--" in the emulator. Compiled in only for the screenshot build.
  if (slot_used(SLOT_HR)) {
    hr_value = 72;
    snprintf(hr_buffer, METRIC_TEXT_SIZE, "%d", hr_value);
  }
  if (slot_used(SLOT_STEPS)) {
    steps_value = 8420;
    snprintf(steps_buffer, METRIC_TEXT_SIZE, "%d.%dk", steps_value / 1000,
             (steps_value % 1000) / 100);
  }
  if (slot_used(SLOT_SLEEP)) {
    sleep_value_min = 450;  // 7:30
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

  GFont time_font;
  if (single_line()) {
    time_font = fit_single_line(time_buffer);
  } else {
    // The worded fitter may insert a newline to force a balanced two-line
    // layout; render the broken string it produces.
    char broken[TIME_BUFFER_SIZE];
    time_font = fit_worded_time(time_buffer, broken, sizeof(broken));
    strncpy(time_buffer, broken, TIME_BUFFER_SIZE - 1);
    time_buffer[TIME_BUFFER_SIZE - 1] = '\0';
  }

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

  // Largest to smallest so the fitter picks the first that fits. Rajdhani is a
  // vector font, crisp at any size, so the ladder is a smooth range. The whole
  // ladder is available to every mode; the worded fitter chooses line count and
  // then the largest font that fills it.
  //
  // FONT_TIME_64 omits 'w'/'W' from its characterRegex: at 64px Rajdhani's 'W'
  // exceeds the SDK's 256px glyph-dimension cap on the non-Emery platforms. No
  // Norwegian/Danish/Swedish time or date word contains w/W, so this is safe;
  // adding a language that uses them means dropping the 64px tier or subsetting
  // differently.
  time_fonts[0] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_64));
  time_fonts[1] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_56));
  time_fonts[2] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_48));
  time_fonts[3] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_42));
  time_fonts[4] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_36));
  time_fonts[5] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_32));
  time_fonts[6] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_28));
  time_fonts[7] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_24));
  time_fonts[8] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_20));
  date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DATE_24));
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
