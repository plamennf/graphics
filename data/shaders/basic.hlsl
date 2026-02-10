cbuffer Per_Scene_Uniforms : register(b0) {
    row_major float4x4 projection_matrix;
    row_major float4x4 view_matrix;
    float padding[32];
};


struct Vertex_Input {
    float2 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct Vertex_Output {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

Vertex_Output vertex_main(Vertex_Input input) {
    Vertex_Output output;

    output.position = mul(projection_matrix, mul(view_matrix, float4(input.position, 0.0, 1.0)));
    output.color    = input.color;
    output.uv       = input.uv;

    return output;
}

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 pixel_main(Vertex_Output input) : SV_TARGET {
    return g_texture.Sample(g_sampler, input.uv) * input.color;
}
