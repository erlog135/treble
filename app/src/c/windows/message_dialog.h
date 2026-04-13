#pragma once

#include <pebble.h>

// ready_state: 1 = no app detected, 2 = app lacks permissions
void message_dialog_push(int ready_state);
