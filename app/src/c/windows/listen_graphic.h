#pragma once

#include <pebble.h>

typedef enum {
  LISTEN_GRAPHIC_STATE_LOOKING,
  LISTEN_GRAPHIC_STATE_FOCUSING,
  LISTEN_GRAPHIC_STATE_FINDING,
} ListenGraphicState;

typedef void (*ListenGraphicStateCallback)(ListenGraphicState state);

// Pixels of the graphic canvas that contain active content at the final frame.
// The canvas is GRAPHIC_HEIGHT tall, but only the top GRAPHIC_ACTIVE_HEIGHT
// pixels are drawn at the found/done state, so text can be centered below it.
#define GRAPHIC_ACTIVE_HEIGHT 70

void listen_graphic_create(Layer *parent_layer);
void listen_graphic_destroy(void);
void listen_graphic_start(void);
void listen_graphic_on_song_found(void);
void listen_graphic_set_state_callback(ListenGraphicStateCallback callback);
Layer *listen_graphic_get_layer(void);
