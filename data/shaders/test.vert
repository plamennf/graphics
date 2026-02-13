#version 460

struct Vertex_Data {
    float x, y, z;
    float u, v;
};

layout (set = 0, binding = 0) readonly buffer Vertices {
    Vertex_Data data[];
} in_vertices;

void main() {
    Vertex_Data vertex = in_vertices.data[gl_VertexIndex];

    vec3 pos = vec3(vertex.x, vertex.y, vertex.z);
    
    gl_Position = vec4(pos, 1.0);
}
