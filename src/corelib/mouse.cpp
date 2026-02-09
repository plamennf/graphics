#include "corelib.h"

bool is_mouse_button_down(Mouse *mouse, Mouse_Button button) {
    return mouse->mouse_button_states[button].is_down;
}

bool is_mouse_button_pressed(Mouse *mouse, Mouse_Button button) {
    return mouse->mouse_button_states[button].is_down && mouse->mouse_button_states[button].changed;    
}

bool was_mouse_button_just_released(Mouse *mouse, Mouse_Button button) {
    return mouse->mouse_button_states[button].was_down && !mouse->mouse_button_states[button].is_down;
}

void set_mouse_button_state(Mouse *mouse, Mouse_Button button, bool is_down) {
    Mouse_Button_State *state = &mouse->mouse_button_states[button];
    state->changed = state->is_down != is_down;
    state->is_down = is_down;
}

void clear_mouse_button_states(Mouse *mouse) {
    for (int i = 0; i < ArrayCount(mouse->mouse_button_states); i++) {
        Mouse_Button_State *state = &mouse->mouse_button_states[i];
        state->was_down = state->is_down;
        state->changed  = false;
    }
}
