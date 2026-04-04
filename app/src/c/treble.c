#include <pebble.h>

// App Keys
#define KEY_COMMAND 0
#define KEY_RESPONSE_RESULT 1
#define KEY_SONG_TITLE 2
#define KEY_SONG_ARTIST 3

// Command & Result Constants
#define CMD_START_RECOGNITION 1
#define RES_SUCCESS 1
#define RES_FAILED 0

static Window *s_main_window;
static TextLayer *s_status_layer;
static TextLayer *s_title_layer;
static TextLayer *s_artist_layer;

static char s_title_buffer[64];
static char s_artist_buffer[64];

// --- Sending Data to Android ---
static void send_recognition_request() {
  // Update UI
  text_layer_set_text(s_status_layer, "Listening...");
  text_layer_set_text(s_title_layer, "");
  text_layer_set_text(s_artist_layer, "");

  // Build and send the dictionary
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox begin failed!");
    return;
  }

  dict_write_int32(iter, KEY_COMMAND, CMD_START_RECOGNITION);
  app_message_outbox_send();
}

// --- Button Click Handlers ---
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_recognition_request();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// --- Receiving Data from Android ---
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *result_tuple = dict_find(iterator, KEY_RESPONSE_RESULT);

  if(result_tuple) {
    int result = result_tuple->value->int32;

    if(result == RES_SUCCESS) {
      text_layer_set_text(s_status_layer, "Found!");

      // Extract and display Title
      Tuple *title_tuple = dict_find(iterator, KEY_SONG_TITLE);
      if(title_tuple) {
        snprintf(s_title_buffer, sizeof(s_title_buffer), "%s", title_tuple->value->cstring);
        text_layer_set_text(s_title_layer, s_title_buffer);
      }

      // Extract and display Artist
      Tuple *artist_tuple = dict_find(iterator, KEY_SONG_ARTIST);
      if(artist_tuple) {
        snprintf(s_artist_buffer, sizeof(s_artist_buffer), "%s", artist_tuple->value->cstring);
        text_layer_set_text(s_artist_layer, s_artist_buffer);
      }
    } else {
      text_layer_set_text(s_status_layer, "No match found.");
      text_layer_set_text(s_title_layer, "");
      text_layer_set_text(s_artist_layer, "");
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed! Reason: %d", (int)reason);
  text_layer_set_text(s_status_layer, "Phone disconnected.");
}

// --- Window Management ---
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_status_layer = text_layer_create(GRect(0, 20, bounds.size.w, 30));
  text_layer_set_text(s_status_layer, "Press SELECT to tag");
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_status_layer));

  s_title_layer = text_layer_create(GRect(0, 60, bounds.size.w, 50));
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_title_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));

  s_artist_layer = text_layer_create(GRect(0, 110, bounds.size.w, 50));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_artist_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_artist_layer);
}

// --- Initialization ---
static void init() {
  s_main_window = window_create();

  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Attach button click configs
  window_set_click_config_provider(s_main_window, click_config_provider);

  window_stack_push(s_main_window, true);

  // Register AppMessage callbacks
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