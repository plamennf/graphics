#include "pch.h"

#ifdef PLATFORM_WINDOWS
#include "renderer_d3d12.h"
#endif

Renderer *renderer_create(Render_Api render_api, Platform_Window *window, bool vsync) {
    switch (render_api) {
#ifdef PLATFORM_WINDOWS
        case RENDER_API_D3D12: {
            return renderer_d3d12_create(window, vsync);
        } break;
#endif

        default: {
            Assert(!"Invalid render api");
            return NULL;
        } break;
    }
}

void renderer_shutdown(Renderer *renderer) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_shutdown((Renderer_D3D12 *)renderer);
        } break;
    }
}

void renderer_execute_render_commands(Renderer *renderer) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_execute_render_commands((Renderer_D3D12 *)renderer);
        } break;
    }
}

void renderer_wait_for_previous_frame(Renderer *renderer) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_wait_for_previous_frame((Renderer_D3D12 *)renderer);
        } break;
    }
}
