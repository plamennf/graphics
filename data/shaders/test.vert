#version 460

struct Vertex_Data {
    float x, y, z;
    float u, v;
    float normal_x, normal_y, normal_z;
    float tangent_x, tangent_y, tangent_z;
    float bitangent_x, bitangent_y, bitangent_z;
};

layout (std430, set = 0, binding = 0) readonly buffer Vertices {
    Vertex_Data data[];
} in_vertices;

layout (set = 0, binding = 1) readonly buffer Indices {
    int data[];
} in_indices;

layout (set = 0, binding = 2) readonly uniform Uniform_Buffer {
    mat4 wvp;
} ubo;

layout (location = 0) out vec2 out_uv;

void main() {
    int index = in_indices.data[gl_VertexIndex];
    Vertex_Data vertex = in_vertices.data[index];

    vec3 pos = vec3(vertex.x, vertex.y, vertex.z);    
    gl_Position = ubo.wvp * vec4(pos, 1.0);

    out_uv = vec2(vertex.u, vertex.v);
}
