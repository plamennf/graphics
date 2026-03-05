#pragma once

struct Key_State {
    bool is_down;
    bool was_down;
    bool changed;
};

bool is_key_down(Key_Code key_code);
bool is_key_pressed(Key_Code key_code);
bool was_key_just_released(Key_Code key_code);

void set_key_state(Key_Code key_code, bool is_down);
void clear_key_states();
