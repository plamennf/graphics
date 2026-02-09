#include "pch.h"
#include "renderer.h"

Global_Variables globals;

int main(int argc, char *argv[]) {
    init_temporary_storage(Kilobytes(40));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };

    ma_init(&globals.permanent_memory, Kilobytes(1));
    
    Platform_Window *window = platform_window_create(0, 0, "Sandbox");
    Renderer *renderer = renderer_create(RENDER_API_D3D12, window, true);
    defer { renderer_shutdown(renderer); };
    
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

        renderer_execute_render_commands(renderer);
        renderer_wait_for_previous_frame(renderer);
    }
    
    return 0;
}
