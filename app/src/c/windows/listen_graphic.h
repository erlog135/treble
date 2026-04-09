#pragma once

#include <pebble.h>

void listen_graphic_create(Layer *parent_layer);
void listen_graphic_destroy(void);
void listen_graphic_start(void);
void listen_graphic_on_song_found(void);
