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

struct Shader_Info {
    Render_Vertex_Type render_vertex_type;
    String filepath;
};

struct Shader {
    Shader_Info info;
};

enum Gpu_Buffer_Type {
    GPU_BUFFER_TYPE_VERTEX_BUFFER,
};

struct Gpu_Buffer {
    Gpu_Buffer_Type type;
    u32 size;
    u32 stride;
};

struct Draw_Item_Info {
    Shader *shader;
    Gpu_Buffer *vertex_buffer;
    Gpu_Buffer *index_buffer;
    int num_indices;
    int first_index;
};

enum Render_Entry_Type : u8 {
    RET_Render_Entry_Draw_Item,
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

void renderer_draw_item(Renderer *renderer, Draw_Item_Info info);
