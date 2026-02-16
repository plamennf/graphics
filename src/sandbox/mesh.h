#pragma once

#include "vulkan_context.h"

struct Material {
    char *diffuse_texture_name;
    char *specular_texture_name;
    char *normal_texture_name;
    
    Vector4 diffuse_color;
    float shininess;
};

struct Submesh {
    int num_indices;
    u32 *indices;
    
    int num_vertices;
    Mesh_Vertex *vertices;
    
    Material material;
    
    u32 vertex_buffer_size = 0;
    u32 index_buffer_size = 0;
    Vulkan_Buffer vertex_index_buffer; // This is immutable and is only read from so it is only one.

    // These are written to and updated every frame, so there are multiple of them
    // so that we can write to one, while the gpu reads from the other
    Vulkan_Buffer uniform_buffers[NUM_FRAMES_IN_FLIGHT]; 
    
    Vulkan_Texture texture;

    void destroy(VkDevice device) {
        vertex_index_buffer.destroy(device);
        texture.destroy(device);
    }    
};

struct Mesh {
    int num_submeshes;
    Submesh *submeshes;
};

bool load_mesh_gltf(Mesh *mesh, const char *filepath);
