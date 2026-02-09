#include "pch.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    init_temporary_storage(Kilobytes(40));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    Platform_Window *window = platform_window_create(0, 0, "Sandbox");
    
    while (window->is_open) {
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
    }
    
    return 0;
}
