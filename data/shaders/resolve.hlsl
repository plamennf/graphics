#include "shader_globals.hlsli"

struct Vertex_Output {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

Vertex_Output vertex_main(Quad_Vertex_Input input) {
    Vertex_Output result;

    result.position = float4(input.position, 0.0, 1.0);
    result.color    = input.color;
    result.uv       = input.uv;
    
    return result;
}

float4 pixel_main(Vertex_Output input) : SV_TARGET {
    float exposure = 2.0;
    
    float4 hdr_color   = albedo_texture.Sample(sampler_linear, input.uv);
    float4 bloom_color = normal_texture.Sample(sampler_linear, input.uv);
    hdr_color.rgb += bloom_color.rgb;
    
    float3 mapped = 1.0 - exp(-hdr_color.rgb * exposure);
    return float4(mapped, 1.0);
}
