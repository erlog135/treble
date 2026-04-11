#include "listen_graphic.h"

#define FIND_TEXT_TRIGGER_FRAME 14

// Graphic canvas dimensions (matches the PDC animation assets)
#define GRAPHIC_WIDTH  150
#define GRAPHIC_HEIGHT 100

// The graphic's vertical center is placed at (window_height / GRAPHIC_CENTER_DIVISOR),
// keeping it in the top third of the window for an uncluttered look.
#define GRAPHIC_CENTER_DIVISOR 3

typedef enum {
  STATE_IDLE,
  STATE_LOOKING,
  STATE_WAITING_FOR_WRAP,
  STATE_FOCUSING,
  STATE_FINDING,
  STATE_DONE
} GraphicState;

static Layer *s_canvas_layer;
static GDrawCommandSequence *s_current_seq;
static AppTimer *s_timer;
static int s_frame_index;
static GraphicState s_state;
static ListenGraphicStateCallback s_state_callback;

static void schedule_next_timer(void);

static void notify_state_change(GraphicState state) {
  if (!s_state_callback) return;
  switch (state) {
    case STATE_LOOKING:
      s_state_callback(LISTEN_GRAPHIC_STATE_LOOKING);
      break;
    case STATE_FOCUSING:
      s_state_callback(LISTEN_GRAPHIC_STATE_FOCUSING);
      break;
    case STATE_FINDING:
      // Deferred: fired from next_frame_handler at FIND_TEXT_TRIGGER_FRAME
      break;
    default:
      break;
  }
}

static void transition_to_sequence(uint32_t resource_id, GraphicState new_state) {
  if (s_current_seq) {
    gdraw_command_sequence_destroy(s_current_seq);
  }
  s_current_seq = gdraw_command_sequence_create_with_resource(resource_id);
  s_frame_index = 0;
  s_state = new_state;
  notify_state_change(new_state);
  layer_mark_dirty(s_canvas_layer);
  schedule_next_timer();
}

static void next_frame_handler(void *context) {
  s_timer = NULL;
  s_frame_index++;

  if (s_state == STATE_FINDING && s_frame_index == FIND_TEXT_TRIGGER_FRAME && s_state_callback) {
    s_state_callback(LISTEN_GRAPHIC_STATE_FINDING);
  }

  int num_frames = gdraw_command_sequence_get_num_frames(s_current_seq);

  if (s_frame_index >= num_frames) {
    switch (s_state) {
      case STATE_LOOKING:
        s_frame_index = 0;
        break;
      case STATE_WAITING_FOR_WRAP:
        transition_to_sequence(RESOURCE_ID_SONG_FOCUS_SEQUENCE, STATE_FOCUSING);
        return;
      case STATE_FOCUSING:
        transition_to_sequence(RESOURCE_ID_SONG_FIND_SEQUENCE, STATE_FINDING);
        return;
      case STATE_FINDING:
        s_frame_index = num_frames - 1;
        s_state = STATE_DONE;
        layer_mark_dirty(s_canvas_layer);
        return;
      default:
        return;
    }
  }

  layer_mark_dirty(s_canvas_layer);
  schedule_next_timer();
}

static void schedule_next_timer(void) {
  GDrawCommandFrame *frame =
      gdraw_command_sequence_get_frame_by_index(s_current_seq, s_frame_index);
  uint32_t duration = gdraw_command_frame_get_duration(frame);
  if (duration == 0) {
    duration = 33;
  }
  s_timer = app_timer_register(duration, next_frame_handler, NULL);
}

static void update_proc(Layer *layer, GContext *ctx) {
  if (!s_current_seq) return;

  GDrawCommandFrame *frame =
      gdraw_command_sequence_get_frame_by_index(s_current_seq, s_frame_index);
  if (frame) {
    gdraw_command_frame_draw(ctx, s_current_seq, frame, GPoint(0, 0));
  }
}

void listen_graphic_create(Layer *parent_layer) {
  GRect bounds = layer_get_bounds(parent_layer);

  // Place the graphic's vertical center at 1/GRAPHIC_CENTER_DIVISOR of the window height.
  // Horizontally center it when the window is wider than the graphic.
  int graphic_center_y = bounds.size.h / GRAPHIC_CENTER_DIVISOR + STATUS_BAR_LAYER_HEIGHT / 2;
  int graphic_y = graphic_center_y - GRAPHIC_HEIGHT / 2;
  int graphic_x = (bounds.size.w > GRAPHIC_WIDTH) ? (bounds.size.w - GRAPHIC_WIDTH) / 2 : 0;

  s_canvas_layer = layer_create(GRect(graphic_x, graphic_y, GRAPHIC_WIDTH, GRAPHIC_HEIGHT));
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(parent_layer, s_canvas_layer);

  s_current_seq = NULL;
  s_timer = NULL;
  s_frame_index = 0;
  s_state = STATE_IDLE;
  s_state_callback = NULL;
}

void listen_graphic_destroy(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  if (s_current_seq) {
    gdraw_command_sequence_destroy(s_current_seq);
    s_current_seq = NULL;
  }
  if (s_canvas_layer) {
    layer_destroy(s_canvas_layer);
    s_canvas_layer = NULL;
  }
  s_state = STATE_IDLE;
  s_state_callback = NULL;
}

void listen_graphic_start(void) {
  transition_to_sequence(RESOURCE_ID_SONG_LOOK_SEQUENCE, STATE_LOOKING);
}

void listen_graphic_on_song_found(void) {
  if (s_state == STATE_LOOKING) {
    s_state = STATE_WAITING_FOR_WRAP;
  }
}

void listen_graphic_set_state_callback(ListenGraphicStateCallback callback) {
  s_state_callback = callback;
}

Layer *listen_graphic_get_layer(void) {
  return s_canvas_layer;
}
