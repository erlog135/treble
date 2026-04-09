#include "listen_window.h"
#include "listen_graphic.h"
#include "notfound_window.h"

static Window *s_listen_window;
static TextLayer *s_looking_layer;
static TextLayer *s_listen_title_layer;
static TextLayer *s_listen_artist_layer;
static PropertyAnimation *s_title_anim;
static PropertyAnimation *s_artist_anim;

static char s_title_buffer[64];
static char s_artist_buffer[64];

static void animate_result_layers() {
  if (s_title_anim) {
    property_animation_destroy(s_title_anim);
    s_title_anim = NULL;
  }
  if (s_artist_anim) {
    property_animation_destroy(s_artist_anim);
    s_artist_anim = NULL;
  }

  Layer *title_layer = text_layer_get_layer(s_listen_title_layer);
  Layer *artist_layer = text_layer_get_layer(s_listen_artist_layer);

  GRect title_from  = GRect(0, 168, 144, 40);
  GRect title_to    = GRect(0, 90,  144, 40);
  GRect artist_from = GRect(0, 168, 144, 30);
  GRect artist_to   = GRect(0, 130, 144, 30);

  layer_set_frame(title_layer,  title_from);
  layer_set_frame(artist_layer, artist_from);

  s_title_anim = property_animation_create_layer_frame(title_layer, &title_from, &title_to);
  animation_set_duration(property_animation_get_animation(s_title_anim), 400);
  animation_set_curve(property_animation_get_animation(s_title_anim), AnimationCurveEaseOut);
  animation_schedule(property_animation_get_animation(s_title_anim));

  s_artist_anim = property_animation_create_layer_frame(artist_layer, &artist_from, &artist_to);
  animation_set_duration(property_animation_get_animation(s_artist_anim), 400);
  animation_set_curve(property_animation_get_animation(s_artist_anim), AnimationCurveEaseOut);
  animation_schedule(property_animation_get_animation(s_artist_anim));
}

static void listen_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_looking_layer = text_layer_create(GRect(0, bounds.size.h - 30, bounds.size.w, 30));
  text_layer_set_text(s_looking_layer, "Looking...");
  text_layer_set_text_alignment(s_looking_layer, GTextAlignmentCenter);
  text_layer_set_font(s_looking_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(window_layer, text_layer_get_layer(s_looking_layer));

  // Title and artist start off-screen below, ready to animate up on result
  s_listen_title_layer = text_layer_create(GRect(0, bounds.size.h, bounds.size.w, 40));
  text_layer_set_font(s_listen_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_listen_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_listen_title_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_listen_title_layer));

  s_listen_artist_layer = text_layer_create(GRect(0, bounds.size.h, bounds.size.w, 30));
  text_layer_set_text_alignment(s_listen_artist_layer, GTextAlignmentCenter);
  text_layer_set_font(s_listen_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(window_layer, text_layer_get_layer(s_listen_artist_layer));

  listen_graphic_create(window_layer);
  listen_graphic_start();
}

static void listen_window_unload(Window *window) {
  listen_graphic_destroy();
  if (s_title_anim) {
    property_animation_destroy(s_title_anim);
    s_title_anim = NULL;
  }
  if (s_artist_anim) {
    property_animation_destroy(s_artist_anim);
    s_artist_anim = NULL;
  }
  text_layer_destroy(s_looking_layer);
  text_layer_destroy(s_listen_title_layer);
  text_layer_destroy(s_listen_artist_layer);
  window_destroy(s_listen_window);
  s_listen_window = NULL;
}

void push_listen_window(void) {
  if (!s_listen_window) {
    s_listen_window = window_create();
    window_set_window_handlers(s_listen_window, (WindowHandlers) {
      .load = listen_window_load,
      .unload = listen_window_unload,
    });
  }
  window_stack_push(s_listen_window, true);
}

void listen_window_on_result(const char *title, const char *artist) {
  snprintf(s_title_buffer, sizeof(s_title_buffer), "%s", title);
  text_layer_set_text(s_listen_title_layer, s_title_buffer);

  snprintf(s_artist_buffer, sizeof(s_artist_buffer), "%s", artist);
  text_layer_set_text(s_listen_artist_layer, s_artist_buffer);

  listen_graphic_on_song_found();
  layer_set_hidden(text_layer_get_layer(s_looking_layer), true);
  animate_result_layers();
}

void listen_window_on_not_found(void) {
  push_notfound_window(s_listen_window);
}
