#include "pch.h"

#include <stdio.h>

Global_Variables globals;

int main(int argc, char *argv[]) {
    init_temporary_storage(Megabytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    globals.window = platform_window_create(0, 0, "Sandbox");

    bool should_show_cursor = true;

    int previous_window_width  = globals.window->width;
    int previous_window_height = globals.window->height;

    float fixed_update_dt = 1.0f / 60.0f;
    u64 last_time = platform_get_time_in_nanoseconds();
    float accumulated_dt = 0.0f;
    while (globals.window->is_open) {
        reset_temporary_storage();

        u64 now_time = platform_get_time_in_nanoseconds();
        u64 dt_ns = now_time - last_time;
        last_time = now_time;

        float dt = (float)((double)dt_ns / 1000000000.0);
        accumulated_dt += dt;
        
        platform_poll_events();

        if (is_key_down(&globals.window->keyboard, KEY_ALT)) {
            if (is_key_pressed(&globals.window->keyboard, KEY_F4)) {
                globals.window->is_open = false;
            } else if (is_key_pressed(&globals.window->keyboard, KEY_ENTER)) {
                platform_window_toggle_fullscreen(globals.window);
            }
        }
        
        if (is_key_pressed(&globals.window->keyboard, KEY_F11)) {
            platform_window_toggle_fullscreen(globals.window);
        }

        if (is_key_pressed(&globals.window->keyboard, KEY_ESCAPE)) {
            should_show_cursor = !should_show_cursor;
        }

        if (globals.window->width  != previous_window_width ||
            globals.window->height != previous_window_height) {
            if (globals.window->width != 0 && globals.window->height != 0) {

                previous_window_width  = globals.window->width;
                previous_window_height = globals.window->height;
            }
        }
        
        if (should_show_cursor) {
            platform_show_and_unlock_cursor();
        } else {
            platform_hide_and_lock_cursor(globals.window);
        }

        if (!should_show_cursor) {
            //update_camera(&camera, CAMERA_TYPE_FPS, dt);
        }

        while (accumulated_dt >= fixed_update_dt) {
            if (!should_show_cursor) {
                //fixed_update_camera(&camera, CAMERA_TYPE_FPS, fixed_update_dt);
            }
            accumulated_dt -= fixed_update_dt;
        }
    }
    
    return 0;
}
