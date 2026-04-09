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

#ifdef DEMO_MODE
static AppTimer *s_demo_timer;

static void demo_timer_callback(void *context) {
  s_demo_timer = NULL;
  listen_window_on_result("Sandstorm", "Darude");
}
#endif

// --- Sending Data to Android ---
static void send_recognition_request() {
#ifdef DEMO_MODE
  if (s_demo_timer) {
    app_timer_cancel(s_demo_timer);
  }
  s_demo_timer = app_timer_register(5000, demo_timer_callback, NULL);
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

  s_prompt_layer = text_layer_create(GRect(0, bounds.size.h / 2 - 15, bounds.size.w, 30));
  text_layer_set_text(s_prompt_layer, "Press SELECT to tag");
  text_layer_set_text_alignment(s_prompt_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_prompt_layer));
}

static void main_window_unload(Window *window) {
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
