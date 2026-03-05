#include "pch.h"

static Key_State key_states[NUM_KEY_CODES];

bool is_key_down(Key_Code key_code) {
    return key_states[key_code].is_down;
}

bool is_key_pressed(Key_Code key_code) {
    return key_states[key_code].is_down && key_states[key_code].changed;
}

bool was_key_just_released(Key_Code key_code) {
    return key_states[key_code].was_down && !key_states[key_code].is_down;
}

void set_key_state(Key_Code key_code, bool is_down) {
    Key_State *state = &key_states[key_code];
    state->changed   = is_down != state->is_down;
    state->is_down   = is_down;
}

void clear_key_states() {
    for (int i = 0; i < ArrayCount(key_states); i++) {
        Key_State *state = &key_states[i];
        state->was_down  = state->is_down;
        state->changed   = false;
    }
}
