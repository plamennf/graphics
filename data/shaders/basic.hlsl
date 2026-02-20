#include "shader_globals.hlsli"

struct Vertex_Output {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
};

Vertex_Output vertex_main(Mesh_Vertex_Input input) {
    Vertex_Output result;

    result.position = mul(projection_matrix, mul(view_matrix, mul(world_matrix, float4(input.position, 1.0))));
    result.uv       = float2(input.uv.x, 1.0 - input.uv.y);
    result.normal   = normalize(input.normal);
    
    return result;
}

float4 pixel_main(Vertex_Output input) : SV_TARGET {
    float3 albedo    = albedo_texture.Sample(sampler_linear, input.uv).rgb;
    float3 normal    = input.normal;//get_normal_from_normal_map();
    float metallic   = metallic_roughness_texture.Sample(sampler_linear, input.uv).b;
    float roughness  = metallic_roughness_texture.Sample(sampler_linear, input.uv).g;
    float ao         = ao_texture.Sample(sampler_linear, input.uv).r;
    
    return float4(albedo, 1.0);
}
