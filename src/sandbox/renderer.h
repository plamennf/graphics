#pragma once

enum Render_Api {
    RENDER_API_D3D12,
};

enum Render_Vertex_Type {
    RENDER_VERTEX_TYPE_IMMEDIATE,
};

struct Immediate_Vertex {
    Vector2 position;
    Vector4 color;
    Vector2 uv;
};

enum Texture_Filter {
    TEXTURE_FILTER_POINT,
    TEXTURE_FILTER_LINEAR,
};

enum Texture_Wrap {
    TEXTURE_WRAP_REPEAT,
    TEXTURE_WRAP_CLAMP,
};

struct Sampler_Info {
    Texture_Filter filter;
    Texture_Wrap wrap;
};

struct Shader_Info {
    Render_Vertex_Type render_vertex_type;
    String filepath;

    int num_static_samplers;
    Sampler_Info *static_samplers;
};

struct Shader {
    Shader_Info info;
};

enum Gpu_Buffer_Type {
    GPU_BUFFER_TYPE_VERTEX_BUFFER,
    GPU_BUFFER_TYPE_INDEX_BUFFER,
};

struct Gpu_Buffer {
    Gpu_Buffer_Type type;
    u32 size;
    u32 stride;
};

enum Texture_Format {
    TEXTURE_FORMAT_UNKNOWN,
    TEXTURE_FORMAT_RGBA8,
};

struct Texture {
    int width;
    int height;

    int bpp;
    Texture_Format format;
};

struct Draw_Item_Info {
    Shader *shader;
    Gpu_Buffer *vertex_buffer;
    Gpu_Buffer *index_buffer;
    Texture *texture;
    int num_indices;
    int first_index;
};

struct Per_Scene_Uniforms {
    Matrix4 projection_matrix;
    Matrix4 view_matrix;
    float padding[32];
};

enum Render_Entry_Type : u8 {
    RET_Render_Entry_Draw_Item,
    RET_Per_Scene_Uniforms,
};

struct Render_Entry_Draw_Item {
    Draw_Item_Info info;
};

struct Renderer {
    static const int NUM_FRAMES = 2;
    
    Render_Api api;

    u32 max_push_buffer_size;
    u8 *push_buffer_base;
    u8 *push_buffer_data_at;
};

Renderer *renderer_create(Render_Api render_api, Platform_Window *window, bool vsync);
void renderer_shutdown(Renderer *renderer);

void renderer_execute_render_commands_and_present(Renderer *renderer);
void renderer_move_to_next_frame(Renderer *renderer);
void renderer_wait_for_gpu(Renderer *renderer);

Shader *renderer_load_shader(Renderer *renderer, Shader_Info info);
Gpu_Buffer *renderer_allocate_buffer(Renderer *renderer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data);
void renderer_update_entire_buffer(Renderer *renderer, Gpu_Buffer *buffer, u32 size, void *data);
Texture *renderer_allocate_texture(Renderer *renderer, int width, int height, Texture_Format format, int bpp, void *pixels);
Texture *renderer_load_texture(Renderer *renderer, String filepath);

void renderer_set_per_scene_uniforms(Renderer *renderer, Per_Scene_Uniforms uniforms);
void renderer_draw_item(Renderer *renderer, Draw_Item_Info info);

void init_immediate_rendering(Renderer *renderer);
void immediate_begin(Renderer *renderer);
void immediate_flush(Renderer *renderer);
void immediate_quad(Renderer *renderer, Texture *texture, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3, Vector4 color);
void immediate_quad(Renderer *renderer, Texture *texture, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector4 color);
void immediate_quad(Renderer *renderer, Texture *texture, Vector2 position, Vector2 size, Vector4 color);
