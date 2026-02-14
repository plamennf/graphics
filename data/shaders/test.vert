#version 460

struct Vertex_Data {
    float x, y, z;
    float u, v;
    float normal_x, normal_y, normal_z;
    float tangent_x, tangent_y, tangent_z;
    float bitangent_x, bitangent_y, bitangent_z;
};

layout (set = 0, binding = 0) readonly uniform Per_Scene_Uniform_Buffer {
    mat4 projection;
    mat4 view;
} per_scene_ubo;

layout (std430, set = 1, binding = 0) readonly buffer Vertices {
    Vertex_Data data[];
} in_vertices;

layout (set = 1, binding = 1) readonly buffer Indices {
    int data[];
} in_indices;

layout (set = 1, binding = 2) readonly uniform Uniform_Buffer {
    mat4 world;
} ubo;

layout (location = 0) out vec2 out_uv;

void main() {
    int index = in_indices.data[gl_VertexIndex];
    Vertex_Data vertex = in_vertices.data[index];

    vec3 pos = vec3(vertex.x, vertex.y, vertex.z);    
    gl_Position = per_scene_ubo.projection * per_scene_ubo.view * ubo.world * vec4(pos, 1.0);

    out_uv = vec2(vertex.u, vertex.v);
}
