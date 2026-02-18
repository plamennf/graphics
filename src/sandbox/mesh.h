#pragma once

#include "renderer.h"

#define MESH_FILE_VERSION 1

struct Mesh_Vertex {
    Vector3 position;
    Vector2 uv;
    Vector3 normal;
    Vector3 tangent;
    Vector3 bitangent;
};

struct Texture;
struct Gpu_Buffer;

struct Material {
    char *diffuse_texture_name;
    Texture *diffuse_texture;
    char *specular_texture_name;
    char *normal_texture_name;
    
    Vector4 diffuse_color;
    float shininess;
    float is_the_cube = 0.0f;
};

struct Submesh {
    int num_indices;
    u32 *indices;
    
    int num_vertices;
    Mesh_Vertex *vertices;

    Gpu_Buffer vertex_buffer;
    Gpu_Buffer index_buffer;
    
    Material material;
};

struct Mesh {
    int num_submeshes;
    Submesh *submeshes;
};

bool load_mesh(Mesh *mesh, String filepath);
bool save_mesh(Mesh *mesh, String filepath);
