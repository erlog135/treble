#include <pebble.h>
#include "windows/listen_window.h"

// App Keys
#define KEY_COMMAND 0
#define KEY_RESPONSE_RESULT 1
#define KEY_SONG_TITLE 2
#define KEY_SONG_ARTIST 3

// Command & Result Constants
#define CMD_START_RECOGNITION 1
#define RES_SUCCESS 1
#define RES_FAILED 0

// Uncomment to simulate a successful recognition after 5 seconds (no phone needed)
#define DEMO_MODE

static Window *s_main_window;
static TextLayer *s_prompt_layer;
static StatusBarLayer *s_status_bar;

#ifdef DEMO_MODE
static AppTimer *s_demo_timer;

static void demo_timer_callback(void *context) {
  s_demo_timer = NULL;
  listen_window_on_result("Sandstorm", "Darude");
  // listen_window_on_result("Super Duper Long Song Name I Wonder How This Will Wrap", "Darude");
  // listen_window_on_result("Sandstorm", "Super Duper Long Artist Name I Wonder How This Will Wrap");
  // listen_window_on_result("Super Duper Long Song Name I Wonder How This Will Wrap", "Super Duper Long Artist Name I Wonder How This Will Wrap");
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
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  push_listen_window();
  send_recognition_request();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar,
    PBL_IF_COLOR_ELSE(GColorFolly, GColorWhite),
    PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  int content_top = STATUS_BAR_LAYER_HEIGHT;
  int content_height = bounds.size.h - content_top;

  s_prompt_layer = text_layer_create(GRect(0, content_top + content_height / 2 - 15, bounds.size.w, 30));
  text_layer_set_text(s_prompt_layer, "Press SELECT to tag");
  text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter);
  text_layer_set_text_color(s_prompt_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_background_color(s_prompt_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_prompt_layer));
}

static void main_window_unload(Window *window) {
  status_bar_layer_destroy(s_status_bar);
  text_layer_destroy(s_prompt_layer);
}

// --- Initialization ---
static void init() {
  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  window_set_click_config_provider(s_main_window, click_config_provider);
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
