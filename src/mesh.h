#pragma once

#include "renderer.h"

#define MESH_FILE_VERSION 1

struct Mesh_Vertex {
    Vector3 position;
    Vector4 color;
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

    char *albedo_texture_name = NULL;
    Texture *albedo_texture = NULL;
    Vector4 albedo_factor = v4(1, 1, 1, 1);
    
    char *normal_texture_name = NULL;
    Texture *normal_texture = NULL;

    char *metallic_roughness_texture_name = NULL;
    Texture *metallic_roughness_texture;

    char *ao_texture_name = NULL;
    Texture *ao_texture = NULL;

    char *emissive_texture_name = NULL;
    Texture *emissive_texture = NULL;
    Vector3 emissive_factor = v3(0, 0, 0);
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
    const char *filename;
    
    int num_submeshes;
    Submesh *submeshes;
};

bool load_mesh(Mesh *mesh, String filepath);
bool save_mesh(Mesh *mesh, String filepath);
