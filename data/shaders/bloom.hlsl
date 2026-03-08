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
    float weight[5] = {
        0.227027,
        0.1945946,
        0.1216216,
        0.054054,
        0.016216
    };

    int horizontal = uses_specular_glossiness;
    
    int width, height;
    albedo_texture.GetDimensions(width, height);
    float2 tex_offset = 1.0 / float2(width, height);
    
    float3 result = albedo_texture.Sample(sampler_linear_clamp, input.uv).rgb * weight[0];
    if (horizontal) {
        [unroll]
        for (int i = 1; i < 5; i++) {
            result += albedo_texture.Sample(sampler_linear_clamp, input.uv + float2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += albedo_texture.Sample(sampler_linear_clamp, input.uv - float2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        [unroll]
        for (int i = 1; i < 5; i++) {
            result += albedo_texture.Sample(sampler_linear_clamp, input.uv + float2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += albedo_texture.Sample(sampler_linear_clamp, input.uv - float2(0.0, tex_offset.y * i)).rgb * weight[i];
        }        
    }

    return float4(result, 1.0);
}
