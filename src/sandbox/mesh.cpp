#include "pch.h"
#include "mesh.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

bool load_mesh_gltf(Mesh *mesh, const char *filepath) {
    cgltf_options options = {};
    cgltf_data *data = NULL;
    //if (cgltf_parse_file(&options, file_data, file_data_size, &data) != cgltf_result_success) {
    if (cgltf_parse_file(&options, filepath, &data) != cgltf_result_success) {
        logprintf("Failed to parse gltf file '%s'.\n", filepath);
        return false;
    }
    defer { cgltf_free(data); };
    if (cgltf_load_buffers(&options, data, filepath) != cgltf_result_success) {
        logprintf("Failed to load buffers for: '%s'\n", filepath);
        return false;
    }
    
    mesh->num_submeshes = 0;
    for (int i = 0; i < data->meshes_count; i++) {
        mesh->num_submeshes += (int)data->meshes[i].primitives_count;
    }

    mesh->submeshes = new Submesh[mesh->num_submeshes];

    int index = 0;
    for (int m = 0; m < data->meshes_count; m++) {
        cgltf_mesh *gltf_mesh = &data->meshes[m];
        for (int p = 0; p < gltf_mesh->primitives_count; p++) {
            cgltf_primitive *primitive = &gltf_mesh->primitives[p];
            Submesh *submesh = &mesh->submeshes[index++];

            cgltf_accessor *position = NULL;
            cgltf_accessor *normal   = NULL;
            cgltf_accessor *uv       = NULL;
            cgltf_accessor *tangent  = NULL;
            for (int a = 0; a < primitive->attributes_count; a++) {
                switch (primitive->attributes[a].type) {
                    case cgltf_attribute_type_position: {
                        position = primitive->attributes[a].data;
                    } break;

                    case cgltf_attribute_type_normal: {
                        normal = primitive->attributes[a].data;
                    } break;

                    case cgltf_attribute_type_texcoord: {
                        if (primitive->attributes[a].index == 0) {
                            uv = primitive->attributes[a].data;
                        }
                    } break;

                    case cgltf_attribute_type_tangent: {
                        tangent = primitive->attributes[a].data;
                    } break;
                }
            }
            if (!position) continue;

            submesh->num_vertices = (int)position->count;
            submesh->vertices = new Mesh_Vertex[submesh->num_vertices];
            
            for (int i = 0; i < submesh->num_vertices; i++) {
                float p_[3] = {};
                float n_[3] = {};
                float u_[2] = {};
                float t_[3] = {};
                cgltf_accessor_read_float(position, i, p_, 3);
                if (normal)  cgltf_accessor_read_float(normal,  i, n_, 3);
                if (uv)      cgltf_accessor_read_float(uv,      i, u_, 2);
                if (tangent) cgltf_accessor_read_float(tangent, i, t_, 3);

                submesh->vertices[i].position = v3(p_[0], p_[1], p_[2]);

                if (normal) {
                    submesh->vertices[i].normal    = v3(n_[0], n_[1], n_[2]);
                } else {
                    submesh->vertices[i].normal    = v3(0, 1, 0);
                }

                if (uv) {
                    submesh->vertices[i].uv        = v2(u_[0], u_[1]);
                } else {
                    submesh->vertices[i].uv        = v2(0, 0);
                }

                if (tangent) {
                    submesh->vertices[i].tangent   = v3(t_[0], t_[1], t_[2]);
                    submesh->vertices[i].bitangent = cross_product(submesh->vertices[i].tangent, submesh->vertices[i].normal);
                } else {
                    submesh->vertices[i].tangent   = v3(0, 0, 0);
                    submesh->vertices[i].bitangent = v3(0, 0, 0);
                }
            }

            submesh->num_indices = primitive->indices ? (int)primitive->indices->count : submesh->num_vertices;
            submesh->indices = new u32[submesh->num_indices];
            for (int i = 0; i < submesh->num_indices; i++) {
                if (primitive->indices) {
                    submesh->indices[i] = (u32)cgltf_accessor_read_index(primitive->indices, i);
                } else {
                    submesh->indices[i] = (u32)i;
                }
            }

            /*
            submesh->vertex_buffer = make_gpu_buffer(GPU_BUFFER_VERTEX, submesh->num_vertices * sizeof(Mesh_Vertex), submesh->vertices, false);
            submesh->index_buffer = make_gpu_buffer(GPU_BUFFER_INDEX, submesh->num_indices * sizeof(u32), submesh->indices, false);
            */
            
            if (primitive->material) {
                cgltf_material *material = primitive->material;

                if (material->pbr_metallic_roughness.base_color_texture.texture) {
                    cgltf_texture *texture = material->pbr_metallic_roughness.base_color_texture.texture;
                    if (texture->image) {
                        char *texture_path_orig = NULL;
                        if (texture->image->uri) {
                            texture_path_orig = texture->image->uri;
                        } else if (texture->image->name) {
                            texture_path_orig = texture->image->name;
                        }

                        if (texture_path_orig) {
                            char *texture_name_orig = copy_string(texture_path_orig);
                            defer { delete [] texture_name_orig; };
                            
                            char *texture_name = strrchr(texture_name_orig, '/');
                            if (texture_name) {
                                texture_name++;
                            } else {
                                texture_name = texture_name_orig;
                            }

                            char *texture_name_end = strrchr(texture_name, '.');
                            if (texture_name_end) {
                                texture_name[texture_name_end - texture_name] = 0;
                            }

                            submesh->material.diffuse_texture_name = copy_string(texture_name);
                        } else {
                            submesh->material.diffuse_texture_name = NULL;
                        }
                    }
                } else {
                    submesh->material.diffuse_texture_name = NULL;
                }

                if (material->pbr_specular_glossiness.diffuse_texture.texture && submesh->material.diffuse_texture_name == NULL) {
                    cgltf_texture *texture = material->pbr_specular_glossiness.diffuse_texture.texture;
                    if (texture->image) {
                        char *texture_path_orig = NULL;
                        if (texture->image->uri) {
                            texture_path_orig = texture->image->uri;
                        } else if (texture->image->name) {
                            texture_path_orig = texture->image->name;
                        }

                        if (texture_path_orig) {
                            char *texture_name_orig = copy_string(texture_path_orig);
                            defer { delete [] texture_name_orig; };
                            
                            char *texture_name = strrchr(texture_name_orig, '/');
                            if (texture_name) {
                                texture_name++;
                            } else {
                                texture_name = texture_name_orig;
                            }

                            char *texture_name_end = strrchr(texture_name, '.');
                            if (texture_name_end) {
                                texture_name[texture_name_end - texture_name] = 0;
                            }

                            submesh->material.diffuse_texture_name = copy_string(texture_name);
                        } else {
                            submesh->material.diffuse_texture_name = NULL;
                        }
                    }
                } else {
                    //submesh->material.diffuse_texture = globals.white_texture;
                }

                if (material->specular.specular_texture.texture) {
                    cgltf_texture *texture = material->specular.specular_texture.texture;
                    if (texture->image) {
                        char *texture_path_orig = NULL;
                        if (texture->image->uri) {
                            texture_path_orig = texture->image->uri;
                        } else if (texture->image->name) {
                            texture_path_orig = texture->image->name;
                        }

                        if (texture_path_orig) {
                            char *texture_name_orig = copy_string(texture_path_orig);
                            defer { delete [] texture_name_orig; };
                            
                            char *texture_name = strrchr(texture_name_orig, '/');
                            if (texture_name) {
                                texture_name++;
                            } else {
                                texture_name = texture_name_orig;
                            }

                            char *texture_name_end = strrchr(texture_name, '.');
                            if (texture_name_end) {
                                texture_name[texture_name_end - texture_name] = 0;
                            }

                            submesh->material.specular_texture_name = copy_string(texture_name);
                        } else {
                            submesh->material.specular_texture_name = NULL;
                        }
                    }
                } else {
                    submesh->material.specular_texture_name = NULL;
                }

                if (material->normal_texture.texture) {
                    cgltf_texture *texture = material->normal_texture.texture;
                    if (texture->image) {
                        char *texture_path_orig = NULL;
                        if (texture->image->uri) {
                            texture_path_orig = texture->image->uri;
                        } else if (texture->image->name) {
                            texture_path_orig = texture->image->name;
                        }

                        if (texture_path_orig) {
                            char *texture_name_orig = copy_string(texture_path_orig);
                            defer { delete [] texture_name_orig; };
                            
                            char *texture_name = strrchr(texture_name_orig, '/');
                            if (texture_name) {
                                texture_name++;
                            } else {
                                texture_name = texture_name_orig;
                            }

                            char *texture_name_end = strrchr(texture_name, '.');
                            if (texture_name_end) {
                                texture_name[texture_name_end - texture_name] = 0;
                            }

                            submesh->material.normal_texture_name = copy_string(texture_name);
                        } else {
                            submesh->material.normal_texture_name = NULL;
                        }
                    }
                } else {
                    submesh->material.normal_texture_name = NULL;
                }

                Vector4 gltf_base_color_factor;
                memcpy(&gltf_base_color_factor.x, material->pbr_metallic_roughness.base_color_factor, 4 * sizeof(float));
                if (length(gltf_base_color_factor) > 1e-6) {
                    submesh->material.diffuse_color = gltf_base_color_factor;
                } else {
                    submesh->material.diffuse_color = v4(1, 1, 1, 1);
                }
                
                float r = material->pbr_metallic_roughness.roughness_factor;
                submesh->material.shininess = 2.0f/(r*r) - 2.0f;
                clamp(&submesh->material.shininess, 1.0f, 256.0f);
            }
        }
    }

    return true;
}
