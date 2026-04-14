#include "history_window.h"
#include "../history_store.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// One section per distinct calendar day, up to one section per song in the worst case
#define MAX_SECTIONS        HISTORY_MAX_SONGS

// Layout constants
#define CELL_MARGIN         7
#define TEXT_BOTTOM_MARGIN  8
#define TITLE_FONT_H        27   // GOTHIC_24_BOLD line height
#define CELL_H_TITLE_ONLY   44
// On round screens keep a wider left margin so text stays clear of the curved bezel
#define SCROLL_LEFT_MARGIN  PBL_IF_ROUND_ELSE(14, CELL_MARGIN)

// Emery and Gabbro are larger-screen platforms – use GOTHIC_24 for the subtitle
// so it isn't dwarfed by the title.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define SUBTITLE_FONT_KEY  FONT_KEY_GOTHIC_24
  #define SUB_FONT_H         27
#else
  #define SUBTITLE_FONT_KEY  FONT_KEY_GOTHIC_18
  #define SUB_FONT_H         21
#endif

// Cell height: title + artist stacked, with a bottom margin only
#define CELL_H_WITH_SUB  (TITLE_FONT_H + SUB_FONT_H + TEXT_BOTTOM_MARGIN)
// Y offset for the artist line (immediately below the title)
#define SUB_Y            TITLE_FONT_H

#define MAX_TEXT_RENDER_W   2000 // must match measurement rect; cell clips the rest

// Marquee scroll timing
#define SCROLL_TIMER_MS       60
#define SCROLL_STEP_PX        2
#define SCROLL_INITIAL_TICKS  25   
#define SCROLL_PAUSE_TICKS    18  

typedef enum { SCROLL_HOLD, SCROLL_RUN, SCROLL_END } ScrollPhase;

// ---- Section / item tables --------------------------------------------------

typedef struct { int start_idx; int num_items; } SectionData;

static Window          *s_history_window;
static StatusBarLayer  *s_history_status_bar;
static MenuLayer       *s_menu_layer;
static bool             s_demo_mode = false;

static SectionData  s_sections[MAX_SECTIONS];
static char         s_section_titles[MAX_SECTIONS][16]; // e.g. "Sat, Apr 11"
static char         s_item_titles[HISTORY_MAX_SONGS][64];
static char         s_item_artists[HISTORY_MAX_SONGS][64];
static int16_t      s_title_widths[HISTORY_MAX_SONGS];  // natural pixel widths
static int16_t      s_artist_widths[HISTORY_MAX_SONGS];
static int          s_num_sections;
static int          s_total_items;
static int          s_cell_content_w;  // set on window load

// ---- Scroll state -----------------------------------------------------------

static AppTimer   *s_scroll_timer;
static int32_t     s_scroll_offset;
static ScrollPhase s_scroll_phase;
static int         s_scroll_ticks;

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
  s_total_items  = 0;

  GFont title_font     = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont sub_font       = fonts_get_system_font(SUBTITLE_FONT_KEY);
  GRect measure_bounds = GRect(0, 0, 2000, 30);

  if (s_demo_mode) {
#if defined(PBL_PLATFORM_APLITE)
    const char *demo_title  = "Lotta Good";
    const char *demo_artist = "Slow Coast";
#elif defined(PBL_PLATFORM_BASALT)
    const char *demo_title  = "Then Comes the Wonder";
    const char *demo_artist = "The Landing";
#elif defined(PBL_PLATFORM_CHALK)
    const char *demo_title  = "Like What I'm Seeing";
    const char *demo_artist = "Layup";
#elif defined(PBL_PLATFORM_DIORITE)
    const char *demo_title  = "&Run";
    const char *demo_artist = "Sir Sly";
#elif defined(PBL_PLATFORM_FLINT)
    const char *demo_title  = "What's It Like Now";
    const char *demo_artist = "Mikky Ekko";
#elif defined(PBL_PLATFORM_EMERY)
    const char *demo_title  = "My Type";
    const char *demo_artist = "Saint Motel";
#elif defined(PBL_PLATFORM_GABBRO)
    const char *demo_title  = "They Don't Make Em Like Me";
    const char *demo_artist = "Pigeon John";
#else
    const char *demo_title  = "Demo Song";
    const char *demo_artist = "Demo Artist";
#endif

    // Thu, Feb 12: platform song + Never Gonna Give You Up
    // Fri, Feb 6:  Sandstorm
    strncpy(s_item_titles[0],  demo_title,                sizeof(s_item_titles[0])  - 1);
    strncpy(s_item_artists[0], demo_artist,               sizeof(s_item_artists[0]) - 1);
    strncpy(s_item_titles[1],  "Never Gonna Give You Up", sizeof(s_item_titles[1])  - 1);
    strncpy(s_item_artists[1], "Rick Astley",             sizeof(s_item_artists[1]) - 1);
    strncpy(s_item_titles[2],  "Sandstorm",               sizeof(s_item_titles[2])  - 1);
    strncpy(s_item_artists[2], "Darude",                  sizeof(s_item_artists[2]) - 1);

    for (int i = 0; i < 3; i++) {
      s_item_titles[i][sizeof(s_item_titles[0])   - 1] = '\0';
      s_item_artists[i][sizeof(s_item_artists[0]) - 1] = '\0';
      GSize ts = graphics_text_layout_get_content_size(
          s_item_titles[i], title_font, measure_bounds,
          GTextOverflowModeWordWrap, GTextAlignmentLeft);
      s_title_widths[i] = ts.w;
      GSize as = graphics_text_layout_get_content_size(
          s_item_artists[i], sub_font, measure_bounds,
          GTextOverflowModeWordWrap, GTextAlignmentLeft);
      s_artist_widths[i] = as.w;
    }

    strncpy(s_section_titles[0], "Sun, Mar 12", sizeof(s_section_titles[0]));
    strncpy(s_section_titles[1], "Thu, Mar 2",  sizeof(s_section_titles[1]));
    s_sections[0]  = (SectionData){ .start_idx = 0, .num_items = 2 };
    s_sections[1]  = (SectionData){ .start_idx = 2, .num_items = 1 };
    s_num_sections = 2;
    s_total_items  = 3;
    return;
  }

  int count = history_store_count();
  if (count == 0) {
    strncpy(s_item_titles[0], "No songs found", sizeof(s_item_titles[0]));
    s_item_artists[0][0] = '\0';
    s_title_widths[0]    = 0;
    s_artist_widths[0]   = 0;
    strncpy(s_section_titles[0], "History", sizeof(s_section_titles[0]));
    s_sections[0]  = (SectionData){ .start_idx = 0, .num_items = 1 };
    s_num_sections = 1;
    s_total_items  = 1;
    return;
  }

  time_t current_day = 0;
  for (int i = 0; i < count; i++) {
    SongRecord rec;
    if (!history_store_get(i, &rec)) continue;

    bool new_section = (s_num_sections == 0) || !same_day(rec.timestamp, current_day);
    if (new_section) {
      format_day(rec.timestamp, s_section_titles[s_num_sections],
                 sizeof(s_section_titles[0]));
      s_sections[s_num_sections] = (SectionData){ .start_idx = s_total_items, .num_items = 0 };
      current_day = rec.timestamp;
      s_num_sections++;
    }

    int idx = s_total_items;
    strncpy(s_item_titles[idx],  rec.title,  sizeof(s_item_titles[0])  - 1);
    strncpy(s_item_artists[idx], rec.artist, sizeof(s_item_artists[0]) - 1);
    s_item_titles[idx][sizeof(s_item_titles[0])   - 1] = '\0';
    s_item_artists[idx][sizeof(s_item_artists[0]) - 1] = '\0';

    GSize ts = graphics_text_layout_get_content_size(
        s_item_titles[idx], title_font, measure_bounds,
        GTextOverflowModeWordWrap, GTextAlignmentLeft);
    s_title_widths[idx] = ts.w;

    if (s_item_artists[idx][0]) {
      GSize as = graphics_text_layout_get_content_size(
          s_item_artists[idx], sub_font, measure_bounds,
          GTextOverflowModeWordWrap, GTextAlignmentLeft);
      s_artist_widths[idx] = as.w;
    } else {
      s_artist_widths[idx] = 0;
    }

    s_sections[s_num_sections - 1].num_items++;
    s_total_items++;
  }
}

// ---------------------------------------------------------------------------
// Marquee scroll helpers
// ---------------------------------------------------------------------------

static void scroll_reset(void) {
  s_scroll_offset = 0;
  s_scroll_phase  = SCROLL_HOLD;
  s_scroll_ticks  = 0;
}

static void advance_scroll(int max_scroll) {
  switch (s_scroll_phase) {
    case SCROLL_HOLD:
      if (++s_scroll_ticks >= SCROLL_INITIAL_TICKS) {
        s_scroll_phase = SCROLL_RUN;
        s_scroll_ticks = 0;
      }
      break;
    case SCROLL_RUN:
      s_scroll_offset += SCROLL_STEP_PX;
      if (s_scroll_offset >= max_scroll) {
        s_scroll_offset = max_scroll;
        s_scroll_phase  = SCROLL_END;
        s_scroll_ticks  = 0;
      }
      layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
      break;
    case SCROLL_END:
      if (++s_scroll_ticks >= SCROLL_PAUSE_TICKS) {
        scroll_reset();
        layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
      }
      break;
  }
}

static void scroll_timer_callback(void *context) {
  s_scroll_timer = NULL;
  if (!s_menu_layer) return;

  MenuIndex idx = menu_layer_get_selected_index(s_menu_layer);
  if (idx.section < (uint16_t)s_num_sections) {
    int item        = s_sections[idx.section].start_idx + idx.row;
    bool has_artist = s_item_artists[item][0] != '\0';
    int max_scroll  = MAX((int)s_title_widths[item],
                          has_artist ? (int)s_artist_widths[item] : 0)
                      - s_cell_content_w;
    if (max_scroll > 0) {
      advance_scroll(max_scroll);
    }
  }

  s_scroll_timer = app_timer_register(SCROLL_TIMER_MS, scroll_timer_callback, NULL);
}

// ---------------------------------------------------------------------------
// MenuLayer callbacks
// ---------------------------------------------------------------------------

static uint16_t get_num_sections_callback(MenuLayer *menu_layer, void *context) {
  return (uint16_t)s_num_sections;
}

static uint16_t get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index,
                                      void *context) {
  if (section_index >= (uint16_t)s_num_sections) return 0;
  return (uint16_t)s_sections[section_index].num_items;
}

static int16_t get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index,
                                        void *context) {
  int item = s_sections[cell_index->section].start_idx + cell_index->row;
  return s_item_artists[item][0] ? CELL_H_WITH_SUB : CELL_H_TITLE_ONLY;
}

static int16_t get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index,
                                          void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void draw_header_callback(GContext *ctx, const Layer *cell_layer,
                                 uint16_t section_index, void *context) {
  GRect bounds = layer_get_bounds(cell_layer);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  // Center header text on round screens; left-align on flat screens.
  GRect text_rect = PBL_IF_ROUND_ELSE(
      GRect(0, -1, bounds.size.w, bounds.size.h),
      GRect(CELL_MARGIN, -1, bounds.size.w - CELL_MARGIN, bounds.size.h));
  GTextAlignment align = PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft);
  graphics_draw_text(ctx, s_section_titles[section_index],
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     text_rect, GTextOverflowModeTrailingEllipsis, align, NULL);
}

// Draw a single scrollable text field.  On round screens, if the text fits
// without scrolling (field_max == 0) it is centered; otherwise it scrolls
// from the left margin as normal.
static void draw_scrollable_text(GContext *ctx, const char *text, GFont font,
                                  int y, int field_h, int cell_w,
                                  int field_max, int field_off) {
#ifdef PBL_ROUND
  if (field_max == 0) {
    graphics_draw_text(ctx, text, font,
        GRect(0, y, cell_w, field_h),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    return;
  }
#endif
  graphics_draw_text(ctx, text, font,
      GRect(SCROLL_LEFT_MARGIN - field_off, y, MAX_TEXT_RENDER_W, field_h),
      GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void draw_row_callback(GContext *ctx, const Layer *cell_layer,
                              MenuIndex *cell_index, void *context) {
  int         item       = s_sections[cell_index->section].start_idx + cell_index->row;
  const char *title      = s_item_titles[item];
  const char *artist     = s_item_artists[item][0] ? s_item_artists[item] : NULL;
  bool        highlighted = menu_cell_layer_is_highlighted(cell_layer);

  // Non-highlighted rows: delegate to the standard draw helper; the MenuLayer
  // has already set the context fill/text colors to the normal palette.
  if (!highlighted) {
    menu_cell_basic_draw(ctx, cell_layer, title, artist, NULL);
    return;
  }

  // Highlighted row: always draw manually so we can apply the scroll offset
  // and avoid trailing ellipsis while the marquee is in its initial hold phase.
  GRect bounds     = layer_get_bounds(cell_layer);
  GFont title_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont sub_font   = fonts_get_system_font(SUBTITLE_FONT_KEY);

  // Fill highlight background (fill color already set by the MenuLayer).
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Per-field scroll offsets, each capped at that field's own overflow.
  int title_max = MAX(0, (int)s_title_widths[item] - s_cell_content_w);
  int title_off = (s_scroll_offset < title_max) ? (int)s_scroll_offset : title_max;

  if (artist) {
    int artist_max = MAX(0, (int)s_artist_widths[item] - s_cell_content_w);
    int artist_off = (s_scroll_offset < artist_max) ? (int)s_scroll_offset : artist_max;

    draw_scrollable_text(ctx, title, title_font,
        0, TITLE_FONT_H + TEXT_BOTTOM_MARGIN, bounds.size.w,
        title_max, title_off);
    draw_scrollable_text(ctx, artist, sub_font,
        SUB_Y, SUB_FONT_H + TEXT_BOTTOM_MARGIN, bounds.size.w,
        artist_max, artist_off);
  } else {
    // Title-only: vertically centered inside CELL_H_TITLE_ONLY
    int center_y = (bounds.size.h - TITLE_FONT_H) / 2;
    draw_scrollable_text(ctx, title, title_font,
        center_y, TITLE_FONT_H + TEXT_BOTTOM_MARGIN, bounds.size.w,
        title_max, title_off);
  }
}

static void draw_background_callback(GContext *ctx, const Layer *bg_layer,
                                     bool highlight, void *context) {
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, layer_get_bounds(bg_layer), 0, GCornerNone);
}

static void selection_changed_callback(MenuLayer *menu_layer, MenuIndex new_index,
                                       MenuIndex old_index, void *context) {
  scroll_reset();
  // The MenuLayer redraws automatically on selection change; no explicit dirty needed.
}

// ---------------------------------------------------------------------------
// Window handlers
// ---------------------------------------------------------------------------

static void history_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  GRect bounds = layer_get_bounds(window_layer);


  // On round screens give the menu the full window so center-focused mode
  // lands on the true geometric center of the display.  The status bar layer
  // is drawn on top and the curved bezel naturally masks the overlap.
  // On flat screens offset the menu below the status bar as usual.
  GRect menu_bounds = PBL_IF_ROUND_ELSE(
      bounds,
      GRect(0, STATUS_BAR_LAYER_HEIGHT,
            bounds.size.w, bounds.size.h - STATUS_BAR_LAYER_HEIGHT));

  // Available text width for the scrolling marquee
  s_cell_content_w = bounds.size.w - SCROLL_LEFT_MARGIN - CELL_MARGIN;

  build_menu_data();
  scroll_reset();

  s_menu_layer = menu_layer_create(menu_bounds);

  menu_layer_set_normal_colors(s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer,
      PBL_IF_COLOR_ELSE(GColorFolly, GColorBlack), GColorWhite);

  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections  = get_num_sections_callback,
    .get_num_rows      = get_num_rows_callback,
    .get_cell_height   = get_cell_height_callback,
    .get_header_height = get_header_height_callback,
    .draw_header       = draw_header_callback,
    .draw_row          = draw_row_callback,
    .draw_background   = draw_background_callback,
    .selection_changed = selection_changed_callback,
  });

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  s_scroll_timer = app_timer_register(SCROLL_TIMER_MS, scroll_timer_callback, NULL);

  // Status bar
  s_history_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_history_status_bar,
      PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite),
      GColorBlack);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_history_status_bar));


}

static void history_window_unload(Window *window) {
  if (s_scroll_timer) {
    app_timer_cancel(s_scroll_timer);
    s_scroll_timer = NULL;
  }
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
  status_bar_layer_destroy(s_history_status_bar);
  s_history_status_bar = NULL;
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

void history_window_set_demo_mode(bool demo) {
  s_demo_mode = demo;
}
