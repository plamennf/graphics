#version 460

struct Vertex_Data {
    float x, y, z;
    float u, v;
};

layout (set = 0, binding = 0) readonly buffer Vertices {
    Vertex_Data data[];
} in_vertices;

layout (set = 0, binding = 1) readonly uniform Uniform_Buffer {
    mat4 wvp;
} ubo;

void main() {
    Vertex_Data vertex = in_vertices.data[gl_VertexIndex];

    vec3 pos = vec3(vertex.x, vertex.y, vertex.z);
    
    gl_Position = ubo.wvp * vec4(pos, 1.0);
}
