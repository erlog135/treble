#include "notfound_window.h"

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define STATUS_FONT_KEY    FONT_KEY_GOTHIC_28_BOLD
  #define STATUS_FONT_LINE_H 28
  #define BOTTOM_TEXT_MARGIN PBL_IF_ROUND_ELSE(36, 24)
#else
  #define STATUS_FONT_KEY    FONT_KEY_GOTHIC_24_BOLD
  #define STATUS_FONT_LINE_H 24
  #define BOTTOM_TEXT_MARGIN PBL_IF_ROUND_ELSE(24, 12)
#endif

#define TEXT_LAYER_PADDING 4
#define NOT_FOUND_GRAPHIC_WIDTH  120
#define NOT_FOUND_GRAPHIC_HEIGHT 120
#define NOT_FOUND_AUTO_CLOSE_MS 15000

static Window *s_notfound_window;
static TextLayer *s_notfound_layer;
static Layer *s_notfound_graphic_layer;
static GDrawCommandSequence *s_not_found_sequence;
static GDrawCommandImage *s_not_found_image;
static AppTimer *s_notfound_timer;
static AppTimer *s_auto_close_timer;
static int s_notfound_frame_index;

static void schedule_next_timer(void);

static void auto_close_notfound_handler(void *context) {
  s_auto_close_timer = NULL;
  if (s_notfound_window && window_stack_contains_window(s_notfound_window)) {
    window_stack_remove(s_notfound_window, true);
  }
}

static void notfound_next_frame_handler(void *context) {
  s_notfound_timer = NULL;

  if (!s_not_found_sequence) {
    return;
  }

  s_notfound_frame_index++;
  int num_frames = gdraw_command_sequence_get_num_frames(s_not_found_sequence);
  if (s_notfound_frame_index >= num_frames) {
    // Loop continuously so the not-found animation keeps running.
    s_notfound_frame_index = 0;
  }

  layer_mark_dirty(s_notfound_graphic_layer);
  schedule_next_timer();
}

static void schedule_next_timer(void) {
  if (!s_not_found_sequence) {
    return;
  }

  GDrawCommandFrame *frame =
      gdraw_command_sequence_get_frame_by_index(s_not_found_sequence, s_notfound_frame_index);
  if (!frame) {
    return;
  }

  uint32_t duration = gdraw_command_frame_get_duration(frame);
  if (duration == 0) {
    duration = 33;
  }
  s_notfound_timer = app_timer_register(duration, notfound_next_frame_handler, NULL);
}

static void notfound_graphic_update_proc(Layer *layer, GContext *ctx) {
#if !defined(PBL_PLATFORM_APLITE)
  if (!s_not_found_sequence) {
    return;
  }

  GDrawCommandFrame *frame =
      gdraw_command_sequence_get_frame_by_index(s_not_found_sequence, s_notfound_frame_index);
  if (frame) {
    gdraw_command_frame_draw(ctx, s_not_found_sequence, frame, GPoint(0, 0));
  }
#else
  if (s_not_found_image) {
    gdraw_command_image_draw(ctx, s_not_found_image, GPointZero);
  }
#endif
}

static void notfound_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int16_t graphic_x = (bounds.size.w - NOT_FOUND_GRAPHIC_WIDTH) / 2;
  s_notfound_graphic_layer = layer_create(
      GRect(graphic_x, 0, NOT_FOUND_GRAPHIC_WIDTH, NOT_FOUND_GRAPHIC_HEIGHT));
  layer_set_update_proc(s_notfound_graphic_layer, notfound_graphic_update_proc);
  layer_add_child(window_layer, s_notfound_graphic_layer);

  int16_t text_h = STATUS_FONT_LINE_H + TEXT_LAYER_PADDING;
  s_notfound_layer = text_layer_create(
      GRect(0, bounds.size.h - text_h - BOTTOM_TEXT_MARGIN, bounds.size.w, text_h));
  text_layer_set_text(s_notfound_layer, "Song not found");
  text_layer_set_text_alignment(s_notfound_layer, GTextAlignmentCenter);
  text_layer_set_font(s_notfound_layer, fonts_get_system_font(STATUS_FONT_KEY));
  text_layer_set_text_color(s_notfound_layer, GColorBlack);
  text_layer_set_background_color(s_notfound_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_notfound_layer));

#if !defined(PBL_PLATFORM_APLITE)
  s_not_found_sequence = gdraw_command_sequence_create_with_resource(RESOURCE_ID_SONG_NOT_FOUND_SEQUENCE);
  s_notfound_frame_index = 0;
  layer_mark_dirty(s_notfound_graphic_layer);
  schedule_next_timer();
#else
  s_not_found_image = gdraw_command_image_create_with_resource(RESOURCE_ID_SONG_NOT_FOUND);
  layer_mark_dirty(s_notfound_graphic_layer);
#endif

  s_auto_close_timer = app_timer_register(NOT_FOUND_AUTO_CLOSE_MS, auto_close_notfound_handler, NULL);
}

static void notfound_window_unload(Window *window) {
  if (s_notfound_timer) {
    app_timer_cancel(s_notfound_timer);
    s_notfound_timer = NULL;
  }
  if (s_auto_close_timer) {
    app_timer_cancel(s_auto_close_timer);
    s_auto_close_timer = NULL;
  }
  if (s_not_found_sequence) {
    gdraw_command_sequence_destroy(s_not_found_sequence);
    s_not_found_sequence = NULL;
  }
  if (s_not_found_image) {
    gdraw_command_image_destroy(s_not_found_image);
    s_not_found_image = NULL;
  }
  if (s_notfound_graphic_layer) {
    layer_destroy(s_notfound_graphic_layer);
    s_notfound_graphic_layer = NULL;
  }
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
