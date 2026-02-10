#include "pch.h"

#ifdef PLATFORM_WINDOWS
#include "renderer_d3d12.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

void renderer_execute_render_commands_and_present(Renderer *renderer) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_execute_render_commands_and_present((Renderer_D3D12 *)renderer);
        } break;
    }
}

void renderer_move_to_next_frame(Renderer *renderer) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_move_to_next_frame((Renderer_D3D12 *)renderer);
        } break;
    }
}

void renderer_wait_for_gpu(Renderer *renderer) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_wait_for_gpu((Renderer_D3D12 *)renderer);
        } break;
    }
}

Shader *renderer_load_shader(Renderer *renderer, Shader_Info info) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            return renderer_d3d12_load_shader((Renderer_D3D12 *)renderer, info);
        } break;
    }

    return NULL;
}

Gpu_Buffer *renderer_allocate_buffer(Renderer *renderer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            return renderer_d3d12_allocate_buffer((Renderer_D3D12 *)renderer, type, size, stride, initial_data);
        } break;
    }

    return NULL;
}

Texture *renderer_allocate_texture(Renderer *renderer, int width, int height, Texture_Format format, int bpp, void *pixels) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            return renderer_d3d12_allocate_texture((Renderer_D3D12 *)renderer, width, height, format, bpp, pixels);
        } break;
    }

    return NULL;
}

Texture *renderer_load_texture(Renderer *renderer, String filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc *data = stbi_load(temp_c_string(filepath), &width, &height, &channels, 4);
    if (!data) {
        logprintf("Failed to load image '%s'\n", filepath);
        return NULL;
    }
    defer { stbi_image_free(data); };

    Texture *result = renderer_allocate_texture(renderer, width, height, TEXTURE_FORMAT_RGBA8, 4, data);
    return result;
}

#define add_render_entry(renderer, Type) (Type *)add_render_entry_(renderer, sizeof(Type), RET_##Type)
inline void *add_render_entry_(Renderer *renderer, u32 size, Render_Entry_Type type) {
    Assert(renderer);
    Assert(renderer->push_buffer_data_at + size + 1 <= renderer->push_buffer_base + renderer->max_push_buffer_size);
    
    *renderer->push_buffer_data_at = (u8)type;
    ++renderer->push_buffer_data_at;

    void *result = (void *)renderer->push_buffer_data_at;
    renderer->push_buffer_data_at += size;
    return result;
}

void renderer_set_per_scene_uniforms(Renderer *renderer, Per_Scene_Uniforms uniforms) {
    auto entry = add_render_entry(renderer, Per_Scene_Uniforms);
    memcpy(entry, &uniforms, sizeof(uniforms));
}

void renderer_draw_item(Renderer *renderer, Draw_Item_Info info) {
    auto entry = add_render_entry(renderer, Render_Entry_Draw_Item);
    memcpy(&entry->info, &info, sizeof(info));
}
