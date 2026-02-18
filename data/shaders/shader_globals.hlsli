cbuffer Per_Scene_Uniforms : register(b0) {
    float4x4 projection_matrix;
    float4x4 view_matrix;
};

cbuffer Per_Object_Uniforms : register(b1) {
    float4x4 world_matrix;
    float4 material_diffuse_color;
    float material_shininess;
}

struct Mesh_Vertex_Input {
    float3 position  : POSITION;
    float2 uv        : TEXCOORD;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
};

SamplerState sampler_point  : register(s0);
SamplerState sampler_linear : register(s1);

Texture2D diffuse_texture : register(t0);
