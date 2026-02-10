struct Vertex_Input {
    float4 position : POSITION;
    float4 color : COLOR;
};

struct Vertex_Output {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

Vertex_Output vertex_main(Vertex_Input input) {
    Vertex_Output output;

    output.position = input.position;
    output.color    = input.color;

    return output;
}

float4 pixel_main(Vertex_Output input) : SV_TARGET {
    return input.color;
}
