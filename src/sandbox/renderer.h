#pragma once

enum Render_Api {
    RENDER_API_D3D12,
};

struct Renderer {
    static const int NUM_FRAMES = 2;
    
    Render_Api api;
};

Renderer *renderer_create(Render_Api render_api, Platform_Window *window, bool vsync);
void renderer_shutdown(Renderer *renderer);

void renderer_execute_render_commands(Renderer *renderer);
void renderer_wait_for_previous_frame(Renderer *renderer);
