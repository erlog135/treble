#include "listen_graphic.h"

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

static void schedule_next_timer(void);

static void transition_to_sequence(uint32_t resource_id, GraphicState new_state) {
  if (s_current_seq) {
    gdraw_command_sequence_destroy(s_current_seq);
  }
  s_current_seq = gdraw_command_sequence_create_with_resource(resource_id);
  s_frame_index = 0;
  s_state = new_state;
  layer_mark_dirty(s_canvas_layer);
  schedule_next_timer();
}

static void next_frame_handler(void *context) {
  s_timer = NULL;
  s_frame_index++;

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
  s_canvas_layer = layer_create(GRect(0, 0, 150, 100));
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(parent_layer, s_canvas_layer);

  s_current_seq = NULL;
  s_timer = NULL;
  s_frame_index = 0;
  s_state = STATE_IDLE;
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
}

void listen_graphic_start(void) {
  transition_to_sequence(RESOURCE_ID_SONG_LOOK_SEQUENCE, STATE_LOOKING);
}

void listen_graphic_on_song_found(void) {
  if (s_state == STATE_LOOKING) {
    s_state = STATE_WAITING_FOR_WRAP;
  }
}
