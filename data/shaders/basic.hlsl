#include "shader_globals.hlsli"

struct Vertex_Output {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

Vertex_Output vertex_main(Mesh_Vertex_Input input) {
    Vertex_Output result;

    result.position = mul(projection_matrix, mul(view_matrix, mul(world_matrix, float4(input.position, 1.0))));
    result.uv       = float2(input.uv.x, 1.0 - input.uv.y);
    
    return result;
}

float4 pixel_main(Vertex_Output input) : SV_TARGET {
    float4 albedo_color = albedo_texture.Sample(sampler_linear, input.uv);
    return albedo_color;// * material_diffuse_color;
}
