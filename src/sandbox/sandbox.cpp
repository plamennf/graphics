#include "pch.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    init_temporary_storage(Kilobytes(40));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    Platform_Window *window = platform_window_create(0, 0, "Sandbox");
    platform_window_toggle_fullscreen(window);
    
    while (window->is_open) {
        platform_poll_events();
    }
    
    return 0;
}
