#include "pch.h"

#include <stdio.h>

Global_Variables globals;

int main(int argc, char *argv[]) {
    init_temporary_storage(Megabytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    if (!platform_window_create(0, 0, "Sandbox")) return false;
    opengl_create_context(4, 5, true, true);
    opengl_set_vsync(true);
    opengl_init_extensions();

    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_MULTISAMPLE);
    
    bool should_show_cursor = true;

    float fixed_update_dt = 1.0f / 60.0f;
    u64 last_time = platform_get_time_in_nanoseconds();
    float accumulated_dt = 0.0f;
    while (platform_window_is_open) {
        reset_temporary_storage();

        u64 now_time = platform_get_time_in_nanoseconds();
        u64 dt_ns = now_time - last_time;
        last_time = now_time;

        float dt = (float)((double)dt_ns / 1000000000.0);
        accumulated_dt += dt;
        
        platform_poll_events();

        if (is_key_down(KEY_ALT)) {
            if (is_key_pressed(KEY_F4)) {
                platform_window_is_open = false;
            } else if (is_key_pressed(KEY_ENTER)) {
                platform_window_toggle_fullscreen();
            }
        }
        
        if (is_key_pressed(KEY_F11)) {
            platform_window_toggle_fullscreen();
        }

        if (is_key_pressed(KEY_ESCAPE)) {
            should_show_cursor = !should_show_cursor;
        }
        
        if (should_show_cursor) {
            platform_show_and_unlock_cursor();
        } else {
            platform_hide_and_lock_cursor();
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

        glClearColor(0.2f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        opengl_swap_buffers();
    }
    
    return 0;
}
