#pragma once

const int MAX_PLATFORM_WINDOWS = 8;

struct Platform_Window {
    int width;
    int height;
    bool is_open;

    Keyboard keyboard;
    Mouse mouse;
};

void platform_init();
void platform_shutdown();

Platform_Window *platform_window_create(int width, int height, String title);
void platform_poll_events();
void *platform_window_get_native(Platform_Window *window);
bool platform_window_was_resized(Platform_Window *window);
void platform_window_toggle_fullscreen(Platform_Window *window);

u64 platform_get_time_in_nanoseconds();
