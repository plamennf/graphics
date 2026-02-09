#pragma once

struct Mouse_Button_State {
    bool is_down;
    bool was_down;
    bool changed;
};

struct Mouse {
    Mouse_Button_State mouse_button_states[NUM_MOUSE_BUTTONS];

    int cursor_x;
    int cursor_y;
    
    int cursor_x_delta;
    int cursor_y_delta;

    int scroll_wheel_x_delta;
    int scroll_wheel_y_delta;
};

bool is_mouse_button_down(Mouse *mouse, Mouse_Button button);
bool is_mouse_button_pressed(Mouse *mouse, Mouse_Button button);
bool was_mouse_button_just_released(Mouse *mouse, Mouse_Button button);

void set_mouse_button_state(Mouse *mouse, Mouse_Button button, bool is_down);
void clear_mouse_button_states(Mouse *mouse);
