#include "listen_window.h"
#include "listen_graphic.h"
#include "notfound_window.h"

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define STATUS_FONT_KEY   FONT_KEY_GOTHIC_28_BOLD
  #define STATUS_FONT_LINE_H 28
  #define BOTTOM_TEXT_MARGIN 18
#else
  #define STATUS_FONT_KEY   FONT_KEY_GOTHIC_24_BOLD
  #define STATUS_FONT_LINE_H 24
  #define BOTTOM_TEXT_MARGIN 12
#endif

#define ARTIST_FONT_LINE_H 18  // FONT_KEY_GOTHIC_18_BOLD

// Distance from the bottom of the window to the bottom of the lowest text layer

// Extra height added to every text layer to prevent descender clipping
#define TEXT_LAYER_PADDING 4

static Window *s_listen_window;
static StatusBarLayer *s_listen_status_bar;
static TextLayer *s_status_layer;
static TextLayer *s_listen_artist_layer;
static PropertyAnimation *s_title_anim;
static PropertyAnimation *s_artist_anim;

static char s_title_buffer[64];
static char s_artist_buffer[70];
static GRect s_window_bounds;

// Custom animation curve for back out with overshoot effect
AnimationProgress animation_back_out_overshoot_curve(AnimationProgress linear_distance) {
  float t = (float)linear_distance / ANIMATION_NORMALIZED_MAX;
  float s = 1.8f;
  t = t - 1.0f;
  float result = t * t * t * (1.0f + s) + t * t * s + 1.0f;
  if (result < 0.0f) result = 0.0f;
  if (result > 1.6f) result = 1.6f;
  return (AnimationProgress)(result * ANIMATION_NORMALIZED_MAX);
}

// artist_lines: 1 or 2. On Emery/Gabbro the artist always respects the bottom
// margin. On other platforms a 2-line artist is allowed to sit flush with the
// window edge to give the extra line room.
static void animate_result_layers(int artist_lines) {
  if (s_title_anim) {
    property_animation_destroy(s_title_anim);
    s_title_anim = NULL;
  }
  if (s_artist_anim) {
    property_animation_destroy(s_artist_anim);
    s_artist_anim = NULL;
  }

  Layer *title_layer  = text_layer_get_layer(s_status_layer);
  Layer *artist_layer = text_layer_get_layer(s_listen_artist_layer);

  GRect title_from = layer_get_frame(title_layer);
  int16_t title_h  = title_from.size.h;
  int16_t artist_h = layer_get_frame(artist_layer).size.h;

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  int16_t artist_margin = BOTTOM_TEXT_MARGIN;
#else
  int16_t artist_margin = (artist_lines == 2) ? 0 : BOTTOM_TEXT_MARGIN;
#endif

  GRect artist_from = GRect(0, s_window_bounds.size.h,                          s_window_bounds.size.w, artist_h);
  GRect artist_to   = GRect(0, s_window_bounds.size.h - artist_h - artist_margin, s_window_bounds.size.w, artist_h);
  GRect title_to    = GRect(0, artist_to.origin.y - title_h + TEXT_LAYER_PADDING,                   s_window_bounds.size.w, title_h);

  layer_set_frame(artist_layer, artist_from);

  s_title_anim = property_animation_create_layer_frame(title_layer, &title_from, &title_to);
  animation_set_duration(property_animation_get_animation(s_title_anim), 400);
  animation_set_custom_curve(property_animation_get_animation(s_title_anim), animation_back_out_overshoot_curve);
  animation_schedule(property_animation_get_animation(s_title_anim));

  s_artist_anim = property_animation_create_layer_frame(artist_layer, &artist_from, &artist_to);
  animation_set_duration(property_animation_get_animation(s_artist_anim), 400);
  animation_set_custom_curve(property_animation_get_animation(s_artist_anim), animation_back_out_overshoot_curve);
  animation_schedule(property_animation_get_animation(s_artist_anim));
}

static void on_graphic_state(ListenGraphicState state) {
  int16_t w = s_window_bounds.size.w;
  int16_t h = s_window_bounds.size.h;

  switch (state) {
    case LISTEN_GRAPHIC_STATE_LOOKING:
    case LISTEN_GRAPHIC_STATE_FOCUSING: {
      const char *label = (state == LISTEN_GRAPHIC_STATE_LOOKING) ? "Looking" : "Finding";
      text_layer_set_text(s_status_layer, label);
      int16_t used_h = STATUS_FONT_LINE_H + TEXT_LAYER_PADDING;
      layer_set_frame(text_layer_get_layer(s_status_layer),
        GRect(0, h - used_h - BOTTOM_TEXT_MARGIN, w, used_h));
      break;
    }
    case LISTEN_GRAPHIC_STATE_FINDING: {
      // Switch to black on yellow when the find animation begins
      window_set_background_color(s_listen_window, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
      status_bar_layer_set_colors(s_listen_status_bar,
        PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite),
        GColorBlack);
      text_layer_set_text_color(s_status_layer, GColorBlack);
      text_layer_set_text_color(s_listen_artist_layer, GColorBlack);

      // --- Size the title layer (preserve current bottom edge) ---
      Layer *title_l = text_layer_get_layer(s_status_layer);
      GRect title_cur = layer_get_frame(title_l);
      int16_t title_bottom = title_cur.origin.y + title_cur.size.h;

      text_layer_set_text(s_status_layer, s_title_buffer);
      // Give it a 2-line frame so text can wrap, then read content height
      layer_set_frame(title_l, GRect(0, 0, w, STATUS_FONT_LINE_H * 2 + TEXT_LAYER_PADDING));
      int title_lines = (text_layer_get_content_size(s_status_layer).h > STATUS_FONT_LINE_H) ? 2 : 1;
      int16_t title_h = STATUS_FONT_LINE_H * title_lines + TEXT_LAYER_PADDING;
      layer_set_frame(title_l, GRect(0, title_bottom - title_h, w, title_h));

      // --- Size the artist layer ---
      Layer *artist_l = text_layer_get_layer(s_listen_artist_layer);
      // Artist starts off-screen; position doesn't matter here, animate_result_layers sets it
      layer_set_frame(artist_l, GRect(0, 0, w, ARTIST_FONT_LINE_H * 2 + TEXT_LAYER_PADDING));
      int artist_lines = (text_layer_get_content_size(s_listen_artist_layer).h > ARTIST_FONT_LINE_H) ? 2 : 1;
      int16_t artist_h = ARTIST_FONT_LINE_H * artist_lines + TEXT_LAYER_PADDING;
      layer_set_frame(artist_l, GRect(0, h, w, artist_h));

      animate_result_layers(artist_lines);
      break;
    }
  }
}

static void listen_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_window_bounds = bounds;

  s_listen_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_listen_status_bar,
    PBL_IF_COLOR_ELSE(GColorSunsetOrange, GColorWhite),
    PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  layer_add_child(window_layer, status_bar_layer_get_layer(s_listen_status_bar));

  // Frames are corrected in on_graphic_state when text is first set
  int16_t status_h = STATUS_FONT_LINE_H + TEXT_LAYER_PADDING;
  s_status_layer = text_layer_create(
    GRect(0, bounds.size.h - status_h - BOTTOM_TEXT_MARGIN, bounds.size.w, status_h));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_font(s_status_layer, fonts_get_system_font(STATUS_FONT_KEY));
  text_layer_set_overflow_mode(s_status_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(s_status_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_background_color(s_status_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_status_layer));

  int16_t artist_h = ARTIST_FONT_LINE_H + TEXT_LAYER_PADDING;
  s_listen_artist_layer = text_layer_create(GRect(0, bounds.size.h, bounds.size.w, artist_h));
  text_layer_set_text_alignment(s_listen_artist_layer, GTextAlignmentCenter);
  text_layer_set_font(s_listen_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_overflow_mode(s_listen_artist_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text_color(s_listen_artist_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_background_color(s_listen_artist_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_listen_artist_layer));

  listen_graphic_create(window_layer);
  listen_graphic_set_state_callback(on_graphic_state);
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
  status_bar_layer_destroy(s_listen_status_bar);
  text_layer_destroy(s_status_layer);
  text_layer_destroy(s_listen_artist_layer);
  window_destroy(s_listen_window);
  s_listen_window = NULL;
}

void push_listen_window(void) {
  if (!s_listen_window) {
    s_listen_window = window_create();
    window_set_background_color(s_listen_window, PBL_IF_COLOR_ELSE(GColorSunsetOrange, GColorWhite));
    window_set_window_handlers(s_listen_window, (WindowHandlers) {
      .load = listen_window_load,
      .unload = listen_window_unload,
    });
  }
  window_stack_push(s_listen_window, true);
}

void listen_window_on_result(const char *title, const char *artist) {
  snprintf(s_title_buffer, sizeof(s_title_buffer), "%s", title);
  snprintf(s_artist_buffer, sizeof(s_artist_buffer), "by %s", artist);
  text_layer_set_text(s_listen_artist_layer, s_artist_buffer);
  listen_graphic_on_song_found();
}

void listen_window_on_not_found(void) {
  push_notfound_window(s_listen_window);
}
