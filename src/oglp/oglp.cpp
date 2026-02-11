#include "pch.h"

int main(int argc, char *argv[]) {
    init_temporary_storage(Kilobytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };

    Platform_Window *window = platform_window_create(0, 0, "OpenGL Playground");
    opengl_create_context(window, 4, 5, true, true);
    opengl_init_extensions();
    opengl_set_vsync(true);
    
    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_MULTISAMPLE);
    
    while (window->is_open) {
        reset_temporary_storage();
        platform_poll_events();
        
        if (is_key_down(&window->keyboard, KEY_ALT)) {
            if (is_key_pressed(&window->keyboard, KEY_F4)) {
                window->is_open = false;
            } else if (is_key_pressed(&window->keyboard, KEY_ENTER)) {
                platform_window_toggle_fullscreen(window);
            }
        }
        
        if (is_key_pressed(&window->keyboard, KEY_F11)) {
            platform_window_toggle_fullscreen(window);
        }

        if (is_key_pressed(&window->keyboard, KEY_ESCAPE)) {
            window->is_open = false;
        }

        glClearColor(0.2f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        opengl_swap_buffers(window);
    }

    return 0;
}
