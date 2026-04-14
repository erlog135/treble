#include "message_dialog.h"

#define DIALOG_MARGIN 8

static const char *s_messages[] = {
  NULL,
  NULL,
  "Can't connect to companion app. Install it from the store page and set it up.",
  "Companion app doesn't have required permissions or background access."
};

static Window *s_window;
static TextLayer *s_label_layer;
static Layer *s_icon_layer;
static GDrawCommandImage *s_icon_image;
static const char *s_message;

static void icon_update_proc(Layer *layer, GContext *ctx) {
  if (s_icon_image) {
    gdraw_command_image_draw(ctx, s_icon_image, GPointZero);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_icon_image = gdraw_command_image_create_with_resource(RESOURCE_ID_GENERIC_WARNING_25PX);
  GSize icon_size = gdraw_command_image_get_bounds_size(s_icon_image);

  int16_t icon_x = PBL_IF_ROUND_ELSE((bounds.size.w - icon_size.w) / 2, DIALOG_MARGIN);
  s_icon_layer = layer_create(GRect(icon_x, DIALOG_MARGIN, icon_size.w, icon_size.h));
  layer_set_update_proc(s_icon_layer, icon_update_proc);
  layer_add_child(window_layer, s_icon_layer);

  s_label_layer = text_layer_create(GRect(
    DIALOG_MARGIN,
    DIALOG_MARGIN + icon_size.h + 5,
    bounds.size.w - (2 * DIALOG_MARGIN),
    bounds.size.h - DIALOG_MARGIN - icon_size.h - 5
  ));
  text_layer_set_text(s_label_layer, s_message);
  text_layer_set_background_color(s_label_layer, GColorClear);
  text_layer_set_text_alignment(s_label_layer, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft));
  text_layer_set_font(s_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_label_layer));
}

static void window_unload(Window *window) {
  layer_destroy(s_icon_layer);
  text_layer_destroy(s_label_layer);
  if (s_icon_image) {
    gdraw_command_image_destroy(s_icon_image);
    s_icon_image = NULL;
  }
  window_destroy(window);
  s_window = NULL;
}

void message_dialog_push(int reason) {
  if (reason < 2 || reason > 3) return;
  s_message = s_messages[reason];

  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
    window_set_window_handlers(s_window, (WindowHandlers){
      .load   = window_load,
      .unload = window_unload
    });
  }
  window_stack_push(s_window, true);
}
