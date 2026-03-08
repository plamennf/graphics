#include "pch.h"
#include "renderer.h"
#include "mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Command_Buffer immediate_cb;

Texture back_buffer;

Texture offscreen_render_target;
Texture offscreen_bloom_target;
Texture offscreen_depth_target;

Texture ping_pong_render_targets[2];

Texture shadow_map_targets[MAX_SHADOW_CASCADES];

bool init_shaders() {
#define LOAD_SHADER(name, type) if (!load_shader(&globals.shader_##name, #name, type)) return false;

    LOAD_SHADER(basic,   RENDER_VERTEX_TYPE_MESH);
    LOAD_SHADER(resolve, RENDER_VERTEX_TYPE_QUAD);
    LOAD_SHADER(bloom,   RENDER_VERTEX_TYPE_QUAD);
    LOAD_SHADER(shadow,  RENDER_VERTEX_TYPE_MESH);
    
#undef LOAD_SHADER
    
    return true;
}

bool load_texture(Texture *texture, String _filepath) {
    Assert(texture);
    
    const char *filepath = temp_c_string(_filepath);
    Assert(filepath);
    
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc *data = stbi_load(filepath, &width, &height, &channels, 4);
    if (!data) {
        logprintf("Failed to load image '%s'\n", filepath);
        return false;
    }
    defer { stbi_image_free(data); };

    if (!create_texture(texture, width, height, TEXTURE_FORMAT_RGBA8, data)) {
        return false;
    }

    return true;
}

bool generate_gpu_data_for_mesh(Mesh *mesh) {
    Assert(mesh);
    
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];

        if (!create_gpu_buffer(&submesh->vertex_buffer, GPU_BUFFER_TYPE_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), sizeof(Mesh_Vertex), submesh->vertices, false)) return false;

        if (!create_gpu_buffer(&submesh->index_buffer, GPU_BUFFER_TYPE_INDEX, submesh->num_indices * sizeof(u32), 0, submesh->indices, false)) return false;

        submesh->material.albedo_texture = globals.white_texture;
        if (submesh->material.albedo_texture_name) {
            submesh->material.albedo_texture = globals.texture_registry->find_or_load(submesh->material.albedo_texture_name);
        }

        submesh->material.normal_texture = globals.white_texture;
        if (submesh->material.normal_texture_name) {
            submesh->material.normal_texture = globals.texture_registry->find_or_load(submesh->material.normal_texture_name);
        }

        submesh->material.metallic_roughness_texture = globals.white_texture;
        if (submesh->material.metallic_roughness_texture_name) {
            submesh->material.metallic_roughness_texture = globals.texture_registry->find_or_load(submesh->material.metallic_roughness_texture_name);
        }

        submesh->material.ao_texture = globals.white_texture;
        if (submesh->material.ao_texture_name) {
            submesh->material.ao_texture = globals.texture_registry->find_or_load(submesh->material.ao_texture_name);
        }

        submesh->material.emissive_texture = globals.white_texture;
        if (submesh->material.emissive_texture_name) {
            submesh->material.emissive_texture = globals.texture_registry->find_or_load(submesh->material.emissive_texture_name);
        }
    }

    return true;
}

void render_mesh(Command_Buffer *cb, Mesh *mesh, Vector3 position, Vector3 rotation, Vector3 scale, Vector4 color) {
    Matrix4 world_matrix = make_transformation_matrix(position, rotation, scale);
    world_matrix = transpose(world_matrix);

    Per_Object_Uniforms uniforms;
    uniforms.world_matrix = world_matrix;
    set_per_object_uniforms(cb, &uniforms);
    
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];

        if (submesh->material.name && strstr(submesh->material.name, "Shadow")) {
            continue;
        }
        
        Render_Item_Info info;
        
        info.vertex_buffer = &submesh->vertex_buffer;
        info.index_buffer  = &submesh->index_buffer;
        info.num_indices   = submesh->num_indices;

        info.albedo_texture = submesh->material.albedo_texture;
        info.normal_texture = submesh->material.normal_texture;
        info.metallic_roughness_texture = submesh->material.metallic_roughness_texture;
        info.ao_texture                 = submesh->material.ao_texture;
        info.emissive_texture           = submesh->material.emissive_texture;
        
        info.uniforms.albedo_factor.x = submesh->material.albedo_factor.x * color.x;
        info.uniforms.albedo_factor.y = submesh->material.albedo_factor.y * color.y;
        info.uniforms.albedo_factor.z = submesh->material.albedo_factor.z * color.z;
        info.uniforms.albedo_factor.w = submesh->material.albedo_factor.w * color.w;
        info.uniforms.has_normal_map  = submesh->material.normal_texture != NULL && submesh->material.normal_texture != globals.white_texture;
        info.uniforms.emissive_factor = submesh->material.emissive_factor;

        info.uniforms.uses_specular_glossiness = submesh->material.uses_specular_glossiness ? 1 : 0;
        
        render_item(cb, &info);
    }
}
