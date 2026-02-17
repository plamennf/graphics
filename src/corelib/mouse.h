#pragma once

struct Mouse_Button_State {
    bool is_down;
    bool was_down;
    bool changed;
};

extern int mouse_cursor_x;
extern int mouse_cursor_y;

extern int mouse_cursor_x_delta;
extern int mouse_cursor_y_delta;

extern int mouse_scroll_wheel_x_delta;
extern int mouse_scroll_wheel_y_delta;

bool is_mouse_button_down(Mouse_Button button);
bool is_mouse_button_pressed(Mouse_Button button);
bool was_mouse_button_just_released(Mouse_Button button);

void set_mouse_button_state(Mouse_Button button, bool is_down);
void clear_mouse_button_states();
