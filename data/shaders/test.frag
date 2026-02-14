#version 460

layout (location = 0) out vec4 output_color;

layout (location = 0) in vec2 in_uv;

layout (set = 0, binding = 2) uniform sampler2D texture_sampler;

void main() {
    vec4 texture_color = texture(texture_sampler, in_uv);
    output_color = texture_color;// * vec4(1.0, 0.5, 0.2, 1.0);
}
