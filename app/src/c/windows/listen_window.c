#include "listen_window.h"
#include "listen_graphic.h"
#include "notfound_window.h"

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define STATUS_FONT_KEY   FONT_KEY_GOTHIC_28_BOLD
  #define STATUS_FONT_LINE_H 28
  #define BOTTOM_TEXT_MARGIN 24
#else
  #define STATUS_FONT_KEY   FONT_KEY_GOTHIC_24_BOLD
  #define STATUS_FONT_LINE_H 24
  #define BOTTOM_TEXT_MARGIN 12
#endif

#define ARTIST_FONT_LINE_H 18  // FONT_KEY_GOTHIC_18_BOLD

// Extra height added to every text layer to prevent descender clipping
#define TEXT_LAYER_PADDING 4

// Margins around the result text block.
// Round watches need larger insets because the screen is circular; the artist
// sits lower than the title so it needs an even wider margin to clear the contour.
#define RESULT_TITLE_SIDE_MARGIN  PBL_IF_ROUND_ELSE(20, 4)
#define RESULT_ARTIST_SIDE_MARGIN PBL_IF_ROUND_ELSE(40, 4)
#define RESULT_BOTTOM_MARGIN      PBL_IF_ROUND_ELSE(10, 0)

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

// Center the title+artist block in the space below the graphic's active area.
static void animate_result_layers(void) {
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

  // x, w, and h for each layer were already set by the FINDING case
  GRect title_from  = layer_get_frame(title_layer);
  GRect artist_cur  = layer_get_frame(artist_layer);
  int16_t title_h   = title_from.size.h;
  int16_t artist_h  = artist_cur.size.h;
  int16_t artist_x  = artist_cur.origin.x;
  int16_t artist_w  = artist_cur.size.w;

  // Center the text block in the space below the graphic's active content,
  // leaving RESULT_BOTTOM_MARGIN clearance at the window bottom.
  GRect graphic_frame = layer_get_frame(listen_graphic_get_layer());
  int16_t space_top    = graphic_frame.origin.y + GRAPHIC_ACTIVE_HEIGHT;
  int16_t available_h  = s_window_bounds.size.h - space_top - RESULT_BOTTOM_MARGIN;
  int16_t text_block_y = space_top + (available_h - title_h - artist_h) / 2;

  GRect artist_from = GRect(artist_x, s_window_bounds.size.h - BOTTOM_TEXT_MARGIN,  artist_w, artist_h);
  GRect title_to    = GRect(title_from.origin.x, text_block_y, title_from.size.w, title_h);
  GRect artist_to   = GRect(artist_x, text_block_y + title_h - TEXT_LAYER_PADDING,  artist_w, artist_h);

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

      int16_t title_x = RESULT_TITLE_SIDE_MARGIN;
      int16_t title_w = w - 2 * RESULT_TITLE_SIDE_MARGIN;

      text_layer_set_text(s_status_layer, s_title_buffer);
      layer_set_frame(title_l, GRect(title_x, 0, title_w, STATUS_FONT_LINE_H * 2 + TEXT_LAYER_PADDING));
      int title_lines = (text_layer_get_content_size(s_status_layer).h > STATUS_FONT_LINE_H) ? 2 : 1;
      int16_t title_h = STATUS_FONT_LINE_H * title_lines + TEXT_LAYER_PADDING;
      layer_set_frame(title_l, GRect(title_x, title_bottom - title_h, title_w, title_h));

      // --- Size the artist layer ---
      // Detect wrapping at title-width first, then pick the effective margin:
      //   both 1-line  → title margin
      //   either 2-line → midpoint
      //   both 2-line  → full artist margin
      Layer *artist_l = text_layer_get_layer(s_listen_artist_layer);
      layer_set_frame(artist_l, GRect(title_x, 0, title_w, ARTIST_FONT_LINE_H * 2 + TEXT_LAYER_PADDING));
      int artist_lines = (text_layer_get_content_size(s_listen_artist_layer).h > ARTIST_FONT_LINE_H) ? 2 : 1;

      int16_t artist_x;
      if (title_lines == 1 && artist_lines == 1) {
        artist_x = RESULT_TITLE_SIDE_MARGIN;
      } else if (title_lines == 2 && artist_lines == 2) {
        artist_x = RESULT_ARTIST_SIDE_MARGIN;
      } else {
        artist_x = (RESULT_TITLE_SIDE_MARGIN + RESULT_ARTIST_SIDE_MARGIN) / 2;
      }
      int16_t artist_w = w - 2 * artist_x;
      int16_t artist_h = ARTIST_FONT_LINE_H * artist_lines + TEXT_LAYER_PADDING;
      layer_set_frame(artist_l, GRect(artist_x, h, artist_w, artist_h));

      animate_result_layers();
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
