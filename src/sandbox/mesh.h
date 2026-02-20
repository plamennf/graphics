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
    /*
    char *diffuse_texture_name;
    Texture *diffuse_texture;
    char *specular_texture_name;
    char *normal_texture_name;
    */

    char *albedo_texture_name;
    Texture *albedo_texture;

    char *normal_texture_name;
    Texture *normal_texture;

    char *metallic_roughness_texture_name;
    Texture *metallic_roughness_texture;

    char *ao_texture_name;
    Texture *ao_texture;
    
    Vector4 diffuse_color;
    float shininess;
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

    int num_lights;
    Light *lights;
};

bool load_mesh(Mesh *mesh, String filepath);
bool save_mesh(Mesh *mesh, String filepath);
