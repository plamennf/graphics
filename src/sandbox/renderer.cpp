#include "pch.h"

#ifdef PLATFORM_WINDOWS
#include "renderer_d3d12.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

const int MAX_IMMEDIATE_VERTICES = 2400;
static Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];
static int num_immediate_vertices;

static Gpu_Buffer *immediate_vbo;
static Texture *immediate_texture;

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

void renderer_update_entire_buffer(Renderer *renderer, Gpu_Buffer *buffer, u32 size, void *data) {
    switch (renderer->api) {
        case RENDER_API_D3D12: {
            renderer_d3d12_update_entire_buffer((Renderer_D3D12 *)renderer, (Gpu_Buffer_D3D12 *)buffer, size, data);
        } break;
    }
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

void init_immediate_rendering(Renderer *renderer) {
    immediate_vbo = renderer_allocate_buffer(renderer, GPU_BUFFER_TYPE_VERTEX_BUFFER, MAX_IMMEDIATE_VERTICES * sizeof(Immediate_Vertex), sizeof(Immediate_Vertex), NULL);
}

void immediate_begin(Renderer *renderer) {
    immediate_flush(renderer);
}

void immediate_flush(Renderer *renderer) {
    if (!num_immediate_vertices) return;

    renderer_update_entire_buffer(renderer, immediate_vbo, num_immediate_vertices * sizeof(Immediate_Vertex), immediate_vertices);

    Draw_Item_Info draw_item_info;
    draw_item_info.shader        = globals.shader_quad;
    draw_item_info.texture       = immediate_texture;
    draw_item_info.vertex_buffer = immediate_vbo;
    draw_item_info.index_buffer  = NULL;
    draw_item_info.num_indices   = num_immediate_vertices;
    draw_item_info.first_index   = 0;
    renderer_draw_item(renderer, draw_item_info);

    num_immediate_vertices = 0;
}

static void put_vertex(Immediate_Vertex *v, Vector2 position, Vector4 color, Vector2 uv) {
    v->position = position;
    v->color    = color;
    v->uv       = uv;
}

void immediate_quad(Renderer *renderer, Texture *texture, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector4 color) {
    if (num_immediate_vertices + 6 > MAX_IMMEDIATE_VERTICES) immediate_flush(renderer);
    if (immediate_texture != texture) immediate_flush(renderer);
    immediate_texture = texture;

    Immediate_Vertex *v = &immediate_vertices[num_immediate_vertices];

    put_vertex(&v[0], p0, color, uv0);
    put_vertex(&v[1], p1, color, uv1);
    put_vertex(&v[2], p2, color, uv2);
    
    put_vertex(&v[3], p0, color, uv0);
    put_vertex(&v[4], p2, color, uv2);
    put_vertex(&v[5], p3, color, uv3);
    
    num_immediate_vertices += 6;
}

void immediate_quad(Renderer *renderer, Texture *texture, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 color) {
    Vector2 uv0 = v2(0, 0);
    Vector2 uv1 = v2(1, 0);
    Vector2 uv2 = v2(1, 1);
    Vector2 uv3 = v2(0, 1);

    immediate_quad(renderer, texture, p0, p1, p2, p3, uv0, uv1, uv2, uv3, color);
}

void immediate_quad(Renderer *renderer, Texture *texture, Vector2 position, Vector2 size, Vector4 color) {
    Vector2 p0 = position;
    Vector2 p1 = v2(position.x + size.x, position.y);
    Vector2 p2 = position + size;
    Vector2 p3 = v2(position.x, position.y + size.y);

    immediate_quad(renderer, texture, p0, p1, p2, p3, color);
}
