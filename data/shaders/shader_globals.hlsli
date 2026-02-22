static const int MAX_LIGHTS = 8;
static const int MAX_SHADOW_CASCADES = 4;

static const float PI = 3.14159265359;

static const int LIGHT_TYPE_UNKNOWN = 0;
static const int LIGHT_TYPE_DIRECTIONAL = 1;
static const int LIGHT_TYPE_POINT = 2;
static const int LIGHT_TYPE_SPOT = 3;

static const int SHADOW_MAP_WIDTH  = 4096;
static const int SHADOW_MAP_HEIGHT = 4096;

struct Light {
    int type;
    float3 _padding0;
    float3 position;
    float _padding1;
    float3 direction;
    float _padding2;
    float3 color;
    float intensity;
    float range;
    float spot_inner_cone_angle;
    float spot_outer_cone_angle;
    float _padding3;
};

cbuffer Per_Scene_Uniforms : register(b0) {
    float4x4 projection_matrix;
    float4x4 view_matrix;
    
    float4x4 light_matrix[MAX_SHADOW_CASCADES];
    float4 cascade_splits[MAX_SHADOW_CASCADES]; // We are wasting memory right now because of hlsl alignment rules. If we end up with a MAX_SHADOW_CASCADES value which is a multiple of 4 we can fix this.

    Light lights[MAX_LIGHTS];

    float3 camera_position;
    int shadow_cascade_index;
};

cbuffer Per_Object_Uniforms : register(b1) {
    float4x4 world_matrix;
}

cbuffer Per_Subobject_Uniforms : register(b2) {
    float4 material_diffuse_color;
    int has_normal_map;
}

struct Mesh_Vertex_Input {
    float3 position  : POSITION;
    float2 uv        : TEXCOORD;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
};

struct Quad_Vertex_Input {
    float2 position  : POSITION;
    float4 color     : COLOR;
    float2 uv        : TEXCOORD;
};

SamplerState sampler_point  : register(s0);
SamplerState sampler_linear : register(s1);

Texture2D albedo_texture : register(t0);
Texture2D normal_texture : register(t1);
Texture2D metallic_roughness_texture : register(t2);
Texture2D ao_texture : register(t3);
Texture2D shadow_textures[MAX_SHADOW_CASCADES] : register(t4);
