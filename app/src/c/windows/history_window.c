#include "history_window.h"
#include "../history_store.h"

// One section per distinct calendar day, up to one section per song in the worst case
#define MAX_SECTIONS HISTORY_MAX_SONGS

static Window          *s_history_window;
static StatusBarLayer  *s_history_status_bar;
static SimpleMenuLayer *s_menu_layer;

// These arrays must outlive the window (SimpleMenuLayer holds raw pointers)
static SimpleMenuItem   s_items[HISTORY_MAX_SONGS];
static SimpleMenuSection s_sections[MAX_SECTIONS];
static char             s_section_titles[MAX_SECTIONS][16]; // e.g. "Sat, Apr 11"
static char             s_item_titles[HISTORY_MAX_SONGS][64];
static char             s_item_artists[HISTORY_MAX_SONGS][64];
static int              s_num_sections;

// ---------------------------------------------------------------------------
// Date helpers
// ---------------------------------------------------------------------------

static const char * const DAY_NAMES[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char * const MONTH_NAMES[] = {"Jan","Feb","Mar","Apr","May",
                                            "Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

static bool same_day(time_t a, time_t b) {
  struct tm *tmp = localtime(&a);
  int ya = tmp->tm_year, da = tmp->tm_yday;
  tmp = localtime(&b);
  return ya == tmp->tm_year && da == tmp->tm_yday;
}

static void format_day(time_t t, char *buf, size_t buf_size) {
  struct tm *tm_info = localtime(&t);
  snprintf(buf, buf_size, "%s, %s %d",
           DAY_NAMES[tm_info->tm_wday],
           MONTH_NAMES[tm_info->tm_mon],
           tm_info->tm_mday);
}

// ---------------------------------------------------------------------------
// Build section/item arrays from persistent storage
// ---------------------------------------------------------------------------

static void build_menu_data(void) {
  s_num_sections = 0;

  int count = history_store_count();
  if (count == 0) {
    strncpy(s_item_titles[0], "No songs found", sizeof(s_item_titles[0]));
    s_items[0] = (SimpleMenuItem){ .title = s_item_titles[0], .subtitle = NULL };
    strncpy(s_section_titles[0], "History", sizeof(s_section_titles[0]));
    s_sections[0] = (SimpleMenuSection){
      .title     = s_section_titles[0],
      .items     = s_items,
      .num_items = 1,
    };
    s_num_sections = 1;
    return;
  }

  int item_idx = 0;
  time_t current_day = 0;

  for (int i = 0; i < count; i++) {
    SongRecord rec;
    if (!history_store_get(i, &rec)) continue;

    bool new_section = (s_num_sections == 0) || !same_day(rec.timestamp, current_day);
    if (new_section) {
      format_day(rec.timestamp, s_section_titles[s_num_sections],
                 sizeof(s_section_titles[0]));
      s_sections[s_num_sections] = (SimpleMenuSection){
        .title     = s_section_titles[s_num_sections],
        .items     = &s_items[item_idx],
        .num_items = 0,
      };
      current_day = rec.timestamp;
      s_num_sections++;
    }

    strncpy(s_item_titles[item_idx],  rec.title,  sizeof(s_item_titles[0])  - 1);
    strncpy(s_item_artists[item_idx], rec.artist, sizeof(s_item_artists[0]) - 1);
    s_item_titles[item_idx][sizeof(s_item_titles[0])   - 1] = '\0';
    s_item_artists[item_idx][sizeof(s_item_artists[0]) - 1] = '\0';

    s_items[item_idx] = (SimpleMenuItem){
      .title    = s_item_titles[item_idx],
      .subtitle = s_item_artists[item_idx][0] ? s_item_artists[item_idx] : NULL,
    };
    s_sections[s_num_sections - 1].num_items++;
    item_idx++;
  }
}

// ---------------------------------------------------------------------------
// Window handlers
// ---------------------------------------------------------------------------

static void history_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorWhite);
  GRect bounds = layer_get_bounds(window_layer);

  build_menu_data();

  GRect menu_bounds = GRect(0, 0, bounds.size.w, bounds.size.h);
  s_menu_layer = simple_menu_layer_create(menu_bounds, window,
                                          s_sections, s_num_sections, NULL);

  MenuLayer *menu_layer = simple_menu_layer_get_menu_layer(s_menu_layer);
  menu_layer_set_normal_colors(menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(menu_layer, PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack), GColorWhite);
  
  layer_add_child(window_layer, simple_menu_layer_get_layer(s_menu_layer));
}

static void history_window_unload(Window *window) {
  simple_menu_layer_destroy(s_menu_layer);
  status_bar_layer_destroy(s_history_status_bar);
  window_destroy(s_history_window);
  s_history_window = NULL;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void push_history_window(void) {
  if (!s_history_window) {
    s_history_window = window_create();
    window_set_window_handlers(s_history_window, (WindowHandlers){
      .load   = history_window_load,
      .unload = history_window_unload,
    });
  }
  window_stack_push(s_history_window, true);
}
