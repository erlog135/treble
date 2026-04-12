#pragma once

#include <pebble.h>

#define HISTORY_MAX_SONGS 10

typedef struct {
  char title[64];
  char artist[64];
  time_t timestamp;
} SongRecord;

void history_store_add(const char *title, const char *artist);
int  history_store_count(void);
bool history_store_get(int index, SongRecord *out);
