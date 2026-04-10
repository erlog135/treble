#pragma once

#include <pebble.h>

typedef enum {
  LISTEN_GRAPHIC_STATE_LOOKING,
  LISTEN_GRAPHIC_STATE_FOCUSING,
  LISTEN_GRAPHIC_STATE_FINDING,
} ListenGraphicState;

typedef void (*ListenGraphicStateCallback)(ListenGraphicState state);

void listen_graphic_create(Layer *parent_layer);
void listen_graphic_destroy(void);
void listen_graphic_start(void);
void listen_graphic_on_song_found(void);
void listen_graphic_set_state_callback(ListenGraphicStateCallback callback);
