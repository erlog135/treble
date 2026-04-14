#include <pebble.h>
#include "windows/listen_window.h"
#include "windows/history_window.h"
#include "windows/message_dialog.h"

// App Keys
#define KEY_COMMAND         0
#define KEY_RESPONSE_RESULT 1
#define KEY_SONG_TITLE      2
#define KEY_SONG_ARTIST     3

// Command & Result Constants
#define CMD_START_RECOGNITION 1
#define RES_SUCCESS    0
#define RES_FAILED     1
#define RES_NO_APP     2
#define RES_NO_PERMS   3

// Uncomment to simulate a successful recognition after 5 seconds (no phone needed)
// #define DEMO_MODE

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define PROMPT_FONT_KEY    FONT_KEY_GOTHIC_28_BOLD
  #define PROMPT_FONT_LINE_H 28
#else
  #define PROMPT_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define PROMPT_FONT_LINE_H 24
#endif

#define PDC_IMAGE_SIZE  80
#define PDC_TOP_OFFSET  24  // pixels above screen center

static Window *s_main_window;
static StatusBarLayer *s_status_bar;
static ActionBarLayer *s_action_bar;
static TextLayer *s_prompt_layer;
static Layer *s_pdc_layer;
static GDrawCommandImage *s_point_right_image;
static GBitmap *s_icon_history;
static GBitmap *s_icon_start;

#ifdef DEMO_MODE
static AppTimer *s_demo_timer;
static bool s_demo_force_not_found;

static void demo_timer_callback(void *context) {
  s_demo_timer = NULL;
  if (s_demo_force_not_found) {
    s_demo_force_not_found = false;
    listen_window_on_not_found();
    return;
  }

  // listen_window_on_result("Sandstorm", "Darude");
  // listen_window_on_result("Super Duper Long Song Name I Wonder How This Will Wrap", "Darude");
  // listen_window_on_result("Sandstorm", "Super Duper Long Artist Name I Wonder How This Will Wrap");
  listen_window_on_result("Super Duper Long Song Name I Wonder How This Will Wrap", "Super Duper Long Artist Name I Wonder How This Will Wrap");
}
#endif

// --- Sending Data to Android ---
static void send_recognition_request() {
#ifdef DEMO_MODE
  if (s_demo_timer) {
    app_timer_cancel(s_demo_timer);
  }
  s_demo_timer = app_timer_register(1000, demo_timer_callback, NULL);
#else
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed!");
    return;
  }

  dict_write_int32(iter, KEY_COMMAND, CMD_START_RECOGNITION);
  app_message_outbox_send();
#endif
}

// --- Receiving Data from Android ---
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *result_tuple = dict_find(iterator, KEY_RESPONSE_RESULT);

  if (result_tuple) {
    int result = result_tuple->value->int32;

    if (result == RES_SUCCESS) {
      Tuple *title_tuple  = dict_find(iterator, KEY_SONG_TITLE);
      Tuple *artist_tuple = dict_find(iterator, KEY_SONG_ARTIST);

      const char *title  = title_tuple  ? title_tuple->value->cstring  : "";
      const char *artist = artist_tuple ? artist_tuple->value->cstring : "";

      listen_window_on_result(title, artist);
    } else if (result == RES_NO_APP || result == RES_NO_PERMS) {
      message_dialog_push(result);
    } else {
      listen_window_on_not_found();
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! Reason: %d", (int)reason);
}

// --- Main Window ---
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  push_history_window();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  push_listen_window();
  send_recognition_request();
}

#ifdef DEMO_MODE
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // In demo mode, DOWN intentionally triggers not-found.
  s_demo_force_not_found = true;
  push_listen_window();
  send_recognition_request();
}
#endif

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
#ifdef DEMO_MODE
  window_single_click_subscribe(BUTTON_ID_DOWN,   down_click_handler);
#endif
}

static void pdc_layer_update_proc(Layer *layer, GContext *ctx) {
  if (s_point_right_image) {
    gdraw_command_image_draw(ctx, s_point_right_image, GPointZero);
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Status bar
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar,
    PBL_IF_COLOR_ELSE(GColorFolly, GColorWhite),
    PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  // Action bar (history at top, start in middle)
  s_icon_history = gbitmap_create_with_resource(RESOURCE_ID_ACTION_ICON_HISTORY);
  s_icon_start   = gbitmap_create_with_resource(RESOURCE_ID_ACTION_ICON_START);

  s_action_bar = action_bar_layer_create();
  action_bar_layer_set_background_color(s_action_bar, GColorBlack);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP,     s_icon_history);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_icon_start);
  action_bar_layer_set_click_config_provider(s_action_bar, click_config_provider);
  action_bar_layer_add_to_window(s_action_bar, window);

  // Content area width (excluding action bar on the right)
  int16_t content_w = bounds.size.w - PBL_IF_ROUND_ELSE(0, ACTION_BAR_WIDTH);

  // PDC image: horizontally centered in content area, top 12px above screen center
  int16_t pdc_top = bounds.size.h / 2 - PDC_TOP_OFFSET;
  int16_t pdc_x   = (content_w - PDC_IMAGE_SIZE) / 2;

  s_point_right_image = gdraw_command_image_create_with_resource(RESOURCE_ID_POINT_RIGHT);

  s_pdc_layer = layer_create(GRect(pdc_x, pdc_top, PDC_IMAGE_SIZE, PDC_IMAGE_SIZE));
  layer_set_update_proc(s_pdc_layer, pdc_layer_update_proc);
  layer_add_child(window_layer, s_pdc_layer);

  // Prompt text "Ready": vertically centered between status bar and top of PDC image
  int16_t text_area_top = STATUS_BAR_LAYER_HEIGHT;
  int16_t text_area_h   = pdc_top - text_area_top;
  int16_t text_h        = PROMPT_FONT_LINE_H + 4;
  int16_t text_y        = text_area_top + (text_area_h - text_h) / 2;

  s_prompt_layer = text_layer_create(GRect(0, text_y, content_w, text_h));
  text_layer_set_text(s_prompt_layer, "Ready");
  text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter);
  text_layer_set_font(s_prompt_layer, fonts_get_system_font(PROMPT_FONT_KEY));
  text_layer_set_text_color(s_prompt_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_background_color(s_prompt_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_prompt_layer));
}

static void main_window_unload(Window *window) {
  status_bar_layer_destroy(s_status_bar);
  action_bar_layer_remove_from_window(s_action_bar);
  action_bar_layer_destroy(s_action_bar);
  gbitmap_destroy(s_icon_history);
  gbitmap_destroy(s_icon_start);
  layer_destroy(s_pdc_layer);
  if (s_point_right_image) {
    gdraw_command_image_destroy(s_point_right_image);
    s_point_right_image = NULL;
  }
  text_layer_destroy(s_prompt_layer);
}

// --- Initialization ---
static void init() {
  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  window_set_background_color(s_main_window, PBL_IF_COLOR_ELSE(GColorFolly, GColorWhite));
  window_stack_push(s_main_window, true);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);

  app_message_open(256, 256);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
