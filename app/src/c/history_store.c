#include "history_store.h"

// Persistent storage keys
#define PERSIST_KEY_COUNT 100
#define PERSIST_KEY_BASE  101  // keys 101–110 for records 0–9 (0 = newest)

void history_store_add(const char *title, const char *artist) {
  int count = persist_exists(PERSIST_KEY_COUNT)
              ? persist_read_int(PERSIST_KEY_COUNT) : 0;

  // Shift existing records towards higher keys (older end).
  // If already at max, the oldest record at index MAX-1 is dropped.
  int shift_count = (count < HISTORY_MAX_SONGS) ? count : HISTORY_MAX_SONGS - 1;
  for (int i = shift_count - 1; i >= 0; i--) {
    SongRecord rec;
    if (persist_read_data(PERSIST_KEY_BASE + i, &rec, sizeof(rec)) == sizeof(rec)) {
      persist_write_data(PERSIST_KEY_BASE + i + 1, &rec, sizeof(rec));
    }
  }

  // Write the new record at index 0 (newest slot)
  SongRecord new_rec;
  memset(&new_rec, 0, sizeof(new_rec));
  strncpy(new_rec.title,  title,  sizeof(new_rec.title)  - 1);
  strncpy(new_rec.artist, artist, sizeof(new_rec.artist) - 1);
  new_rec.timestamp = time(NULL);
  persist_write_data(PERSIST_KEY_BASE, &new_rec, sizeof(new_rec));

  int new_count = (count < HISTORY_MAX_SONGS) ? count + 1 : HISTORY_MAX_SONGS;
  persist_write_int(PERSIST_KEY_COUNT, new_count);
}

int history_store_count(void) {
  return persist_exists(PERSIST_KEY_COUNT)
         ? persist_read_int(PERSIST_KEY_COUNT) : 0;
}

bool history_store_get(int index, SongRecord *out) {
  if (index < 0 || index >= HISTORY_MAX_SONGS) return false;
  if (!persist_exists(PERSIST_KEY_BASE + index))  return false;
  return persist_read_data(PERSIST_KEY_BASE + index, out, sizeof(SongRecord))
         == sizeof(SongRecord);
}
