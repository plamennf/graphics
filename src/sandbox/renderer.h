#pragma once

#ifdef RENDER_D3D11
#include "renderer_d3d11.h"
#endif

#define SHADER_DIRECTORY "data/shaders/compiled"

struct Mesh;

struct Per_Scene_Uniforms {
    Matrix4 projection_matrix;
    Matrix4 view_matrix;
};

enum Render_Vertex_Type {
    RENDER_VERTEX_TYPE_MESH,
};

enum Gpu_Buffer_Type {
    GPU_BUFFER_TYPE_VERTEX,
    GPU_BUFFER_TYPE_INDEX,
    GPU_BUFFER_TYPE_CONSTANT,
};

struct Gpu_Buffer : public Gpu_Buffer_Platform_Specific {
    Gpu_Buffer_Type type;
    u32 size;
    u32 stride;
    bool is_dynamic;
};

enum Texture_Format {
    TEXTURE_FORMAT_UNKNOWN,

    TEXTURE_FORMAT_RGBA8,
    
    TEXTURE_FORMAT_D32,
};

struct Texture : public Texture_Platform_Specific {
    int width;
    int height;

    Texture_Format format;
    int bytes_per_pixel;
};

void init_renderer(bool vsync);
void resize_renderer();
void render_frame_and_present(Vector4 clear_color);

bool create_gpu_buffer(Gpu_Buffer *buffer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data, bool is_dynamic);
bool create_texture(Texture *texture, int width, int height, Texture_Format format, u8 *initial_data);
bool load_shader(Shader *shader, String filename, Render_Vertex_Type vertex_type);

// API agnostic rendering functions
bool load_texture(Texture *texture, String filepath);
bool generate_gpu_data_for_mesh(Mesh *mesh);

void set_per_scene_uniforms(Per_Scene_Uniforms uniforms);
void render_mesh(Mesh *mesh, Vector3 position, Vector3 rotation, Vector3 scale, Vector4 color);
