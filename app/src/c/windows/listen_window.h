#pragma once

#include <pebble.h>

void push_listen_window(void);
void listen_window_on_result(const char *title, const char *artist);
void listen_window_on_not_found(void);
void listen_window_set_demo_mode(bool demo);
