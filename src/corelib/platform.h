#pragma once

extern int platform_window_width;
extern int platform_window_height;
extern bool platform_window_is_open;

void platform_init();
void platform_shutdown();

bool platform_window_create(int width, int height, String title);
void platform_poll_events();
void *platform_window_get_native();
bool platform_window_was_resized();
void platform_window_toggle_fullscreen();

u64 platform_get_time_in_nanoseconds();

void platform_show_and_unlock_cursor();
void platform_hide_and_lock_cursor();
