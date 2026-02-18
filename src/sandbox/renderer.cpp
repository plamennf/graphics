#include "pch.h"
#include "renderer.h"
#include "renderer_internal.h"
#include "mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Shader shader_basic;

Render_Command *render_commands = NULL;
int num_render_commands = 0;

bool init_shaders() {
#define LOAD_SHADER(name, type) if (!load_shader(&shader_##name, #name, type)) return false;

    LOAD_SHADER(basic, RENDER_VERTEX_TYPE_MESH);
    
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

        submesh->material.diffuse_texture = globals.texture_registry->find_or_load(submesh->material.diffuse_texture_name);
    }

    return true;
}

void set_per_scene_uniforms(Per_Scene_Uniforms uniforms) {
    uniforms.projection_matrix = transpose(uniforms.projection_matrix);
    uniforms.view_matrix       = transpose(uniforms.view_matrix);
    add_per_scene_uniforms(&uniforms);
}

void render_mesh(Mesh *mesh, Vector3 position, Vector3 rotation, Vector3 scale, Vector4 color) {
    Matrix4 world_matrix = make_transformation_matrix(position, rotation, scale);
    world_matrix = transpose(world_matrix);
    
    for (int i = 0; i < mesh->num_submeshes; i++) {
        Submesh *submesh = &mesh->submeshes[i];
        
        Render_Mesh_Info info;

        info.vertex_buffer = &submesh->vertex_buffer;
        info.index_buffer  = &submesh->index_buffer;
        info.num_indices   = submesh->num_indices;

        info.diffuse_texture = submesh->material.diffuse_texture;
        
        info.uniforms.world_matrix    = world_matrix;
        info.uniforms.diffuse_color.x = submesh->material.diffuse_color.x * color.x;
        info.uniforms.diffuse_color.y = submesh->material.diffuse_color.y * color.y;
        info.uniforms.diffuse_color.z = submesh->material.diffuse_color.z * color.z;
        info.uniforms.diffuse_color.w = submesh->material.diffuse_color.w * color.w;
        info.uniforms.shininess       = submesh->material.shininess;
        info.uniforms.is_the_cube     = submesh->material.is_the_cube;
        
        add_render_mesh_info(&info);
    }
}
