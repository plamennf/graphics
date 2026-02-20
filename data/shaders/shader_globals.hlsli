cbuffer Per_Scene_Uniforms : register(b0) {
    float4x4 projection_matrix;
    float4x4 view_matrix;
};

cbuffer Per_Object_Uniforms : register(b1) {
    float4x4 world_matrix;
}

cbuffer Per_Subobject_Uniforms : register(b2) {
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
