#include "corelib.h"

bool is_key_down(Keyboard *keyboard, Key_Code key_code) {
    return keyboard->key_states[key_code].is_down;
}

bool is_key_pressed(Keyboard *keyboard, Key_Code key_code) {
    return keyboard->key_states[key_code].is_down && keyboard->key_states[key_code].changed;
}

bool was_key_just_released(Keyboard *keyboard, Key_Code key_code) {
    return keyboard->key_states[key_code].was_down && !keyboard->key_states[key_code].is_down;
}

void set_key_state(Keyboard *keyboard, Key_Code key_code, bool is_down) {
    Key_State *state = &keyboard->key_states[key_code];
    state->changed   = is_down != state->is_down;
    state->is_down   = is_down;
}

void clear_key_states(Keyboard *keyboard) {
    for (int i = 0; i < ArrayCount(keyboard->key_states); i++) {
        Key_State *state = &keyboard->key_states[i];
        state->was_down  = state->is_down;
        state->changed   = false;
    }
}
