#pragma once

#include <pebble.h>

// reason: RES_NO_APP (2) = no app detected, RES_NO_PERMS (3) = app lacks permissions
void message_dialog_push(int reason);
