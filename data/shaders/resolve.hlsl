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
    const float gamma = 2.2;
    float4 hdr_color = albedo_texture.Sample(sampler_point, input.uv);

    float3 mapped = hdr_color.rgb / (hdr_color.rgb + float3(1.0, 1.0, 1.0));
    //mapped = pow(mapped, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));
    
    return float4(mapped, 1.0);
}
