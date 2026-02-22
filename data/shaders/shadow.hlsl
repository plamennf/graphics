#include "shader_globals.hlsli"

struct Vertex_Output {
    float4 position : SV_POSITION;
};

Vertex_Output vertex_main(Mesh_Vertex_Input input) {
    Vertex_Output result;

    switch (shadow_cascade_index) {
        case 0: {
            result.position = mul(light_matrix[0], mul(world_matrix, float4(input.position, 1.0)));
        } break;

        case 1: {
            result.position = mul(light_matrix[1], mul(world_matrix, float4(input.position, 1.0)));
        } break;

        case 2: {
            result.position = mul(light_matrix[2], mul(world_matrix, float4(input.position, 1.0)));
        } break;

        case 3: {
            result.position = mul(light_matrix[3], mul(world_matrix, float4(input.position, 1.0)));
        } break;
    }
    
    
    return result;
}

float4 pixel_main(Vertex_Output input) : SV_TARGET {
    return float4(1.0, 1.0, 1.0, 1.0);
}
