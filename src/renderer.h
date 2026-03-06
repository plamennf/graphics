#pragma once

#ifdef RENDER_D3D11
#include "renderer_d3d11.h"
#endif

#define SHADER_DIRECTORY "data/shaders/compiled"

struct Mesh;

const int MAX_LIGHTS = 8;
const int MAX_SHADOW_CASCADES = 4;

const int SHADOW_MAP_WIDTH  = 4096;
const int SHADOW_MAP_HEIGHT = SHADOW_MAP_WIDTH;

enum Light_Type : int {
    LIGHT_TYPE_UNKNOWN,
    LIGHT_TYPE_DIRECTIONAL,
    LIGHT_TYPE_POINT,
    LIGHT_TYPE_SPOT,
};

struct Light {
    Light_Type type;
    int _padding0[3];
    Vector3 position;
    float _padding1;
    Vector3 direction;
    float _padding2;
    Vector3 color;
    float intensity;
    float range;
    float spot_inner_cone_angle;
    float spot_outer_cone_angle;
    float _padding3;
};
static_assert(sizeof(Light) % 16 == 0, "Light struct must be 16-byte aligned");

struct Quad_Vertex {
    Vector2 position;
    Vector4 color;
    Vector2 uv;
};

struct Per_Scene_Uniforms {
    Matrix4 projection_matrix;
    Matrix4 view_matrix;
    Matrix4 light_matrix[MAX_SHADOW_CASCADES];
    Vector4 cascade_splits[MAX_SHADOW_CASCADES];  // We are wasting memory right now because of hlsl alignment rules. If we end up with a MAX_SHADOW_CASCADES value which is a multiple of 4 we can fix this.
    Light lights[MAX_LIGHTS];
    Vector3 camera_position;
    int shadow_cascade_index;
    //float padding[12];
};

enum Render_Vertex_Type {
    RENDER_VERTEX_TYPE_MESH,
    RENDER_VERTEX_TYPE_QUAD,
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

struct Per_Object_Uniforms {
    Matrix4 world_matrix;
};

struct Per_Subobject_Uniforms {
    Vector4 albedo_factor;
    int has_normal_map;
    Vector3 emissive_factor;
};

struct Render_Item_Info {
    Gpu_Buffer *vertex_buffer;
    Gpu_Buffer *index_buffer;
    int num_indices;

    Texture *albedo_texture;
    Texture *normal_texture;
    Texture *metallic_roughness_texture;
    Texture *ao_texture;
    Texture *emissive_texture;
    
    Per_Subobject_Uniforms uniforms;
};

enum Render_Pipeline_Type {
    RENDER_PIPELINE_MESH,
    RENDER_PIPELINE_QUAD,
    RENDER_PIPELINE_SHADOW,
};

enum Texture_Type {
    TEXTURE_ALBEDO,
    TEXTURE_NORMAL,
    TEXTURE_METALLIC_ROUGHNESS,
    TEXTURE_AO,
    TEXTURE_EMISSIVE,
    TEXTURE_SHADOW_MAP,
};

struct Command_Buffer : public Command_Buffer_Platform_Specific {
    // In the future we'll probably have multiple threads each with their own command buffers, so each will probably need a set of resources.
    Gpu_Buffer per_scene_cb;
    Gpu_Buffer per_object_cb;
    Gpu_Buffer per_subobject_cb;
};

extern Command_Buffer immediate_cb;

extern Texture back_buffer;

extern Texture offscreen_render_target;
extern Texture offscreen_depth_target;

extern Texture shadow_map_targets[MAX_SHADOW_CASCADES];

void init_renderer(bool vsync);
void shutdown_renderer();

void resize_renderer();
void render_frame(int num_command_buffers, Command_Buffer *cbs);
void swap_buffers();

bool init_command_buffer(Command_Buffer *cb);
void release_command_buffer(Command_Buffer *cb);

void set_render_targets(Command_Buffer *cb, int num_render_targets, Texture *render_targets, Texture *depth_target);
void clear_render_target(Command_Buffer *cb, Texture *render_target, Vector4 clear_color);
void set_viewport(Command_Buffer *cb, int width, int height);
void clear_depth_target(Command_Buffer *cb, Texture *depth_target, float z, u8 stencil);
void set_pipeline_type(Command_Buffer *cb, Render_Pipeline_Type type);
void set_per_scene_uniforms(Command_Buffer *cb, Per_Scene_Uniforms *uniforms);
void set_per_object_uniforms(Command_Buffer *cb, Per_Object_Uniforms *uniforms);
void resolve_render_targets(Command_Buffer *cb, Texture *source, Texture *destination);
void render_item(Command_Buffer *cb, Render_Item_Info *info);
void set_texture(Command_Buffer *cb, Texture_Type type, Texture *texture);

bool create_gpu_buffer(Gpu_Buffer *buffer, Gpu_Buffer_Type type, u32 size, u32 stride, void *initial_data, bool is_dynamic);
void release_gpu_buffer(Gpu_Buffer *buffer);
bool create_texture(Texture *texture, int width, int height, Texture_Format format, u8 *initial_data);
void release_texture(Texture *texture);
bool load_shader(Shader *shader, String filename, Render_Vertex_Type vertex_type);
void release_shader(Shader *shader);

// API agnostic rendering functions
bool load_texture(Texture *texture, String filepath);
bool generate_gpu_data_for_mesh(Mesh *mesh);

void render_mesh(Command_Buffer *cb, Mesh *mesh, Vector3 position, Vector3 rotation, Vector3 scale, Vector4 color);
