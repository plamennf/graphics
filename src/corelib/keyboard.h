#pragma once

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

struct Keyboard {
    Key_State key_states[NUM_KEY_CODES];
};

bool is_key_down(Keyboard *keyboard, Key_Code key_code);
bool is_key_pressed(Keyboard *keyboard, Key_Code key_code);
bool was_key_just_released(Keyboard *keyboard, Key_Code key_code);

void set_key_state(Keyboard *keyboard, Key_Code key_code, bool is_down);
void clear_key_states(Keyboard *keyboard);
