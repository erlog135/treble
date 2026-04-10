#include "notfound_window.h"

// Distance from the bottom of the window to the bottom of the text layer
#define BOTTOM_TEXT_MARGIN 8

static Window *s_notfound_window;
static StatusBarLayer *s_notfound_status_bar;
static TextLayer *s_notfound_layer;

static void notfound_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_notfound_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_notfound_status_bar,
    PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite),
    GColorBlack);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_notfound_status_bar));

  s_notfound_layer = text_layer_create(GRect(0, bounds.size.h - 36 - BOTTOM_TEXT_MARGIN, bounds.size.w, 36));
  text_layer_set_text(s_notfound_layer, "Song not found");
  text_layer_set_text_alignment(s_notfound_layer, GTextAlignmentCenter);
  text_layer_set_font(s_notfound_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(s_notfound_layer, GColorBlack);
  text_layer_set_background_color(s_notfound_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_notfound_layer));
}

static void notfound_window_unload(Window *window) {
  status_bar_layer_destroy(s_notfound_status_bar);
  text_layer_destroy(s_notfound_layer);
  window_destroy(s_notfound_window);
  s_notfound_window = NULL;
}

void push_notfound_window(Window *window_to_remove) {
  if (!s_notfound_window) {
    s_notfound_window = window_create();
    window_set_background_color(s_notfound_window, PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite));
    window_set_window_handlers(s_notfound_window, (WindowHandlers) {
      .load = notfound_window_load,
      .unload = notfound_window_unload,
    });
  }
  // Push before removing so the stack is never empty
  window_stack_push(s_notfound_window, true);
  window_stack_remove(window_to_remove, false);
}
