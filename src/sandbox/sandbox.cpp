#include "pch.h"
#include "vulkan_context.h"
#include "mesh.h"
#include "camera.h"

#include <stdio.h>

struct Per_Scene_Uniform_Data {
    Matrix4 projection_matrix;
    Matrix4 view_matrix;
};

struct Per_Object_Uniform_Data {
    Matrix4 world_matrix;
};

struct Vertex {
    Vector3 position;
    Vector2 uv;
};

Global_Variables globals;

static bool file_exists(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

int main(int argc, char *argv[]) {
    init_temporary_storage(Megabytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    globals.window = platform_window_create(0, 0, "Sandbox");

    Vulkan_Context context;
    if (!context.init(globals.window)) return 1;

    int num_images = context.images.count;
    Array <VkCommandBuffer> command_buffers;
    command_buffers.resize(num_images);
    if (!context.create_command_buffers(num_images, command_buffers.data)) return 1;
    Vulkan_Queue *command_queue = &context.command_queue;

    VkShaderModule vs = vulkan_create_shader_module_from_binary(context.device, "data/shaders/compiled/test.vert.spv");
    VkShaderModule fs = vulkan_create_shader_module_from_binary(context.device, "data/shaders/compiled/test.frag.spv");
    
    Vertex vertices[] = {
        { { -1.0f, -1.0f, -5.0f }, { 0.0f, 0.0f } },
        { { -1.0f, +1.0f, -5.0f }, { 0.0f, 1.0f } },
        { { +1.0f, +1.0f, -5.0f }, { 1.0f, 1.0f } },
        { { -1.0f, -1.0f, -5.0f }, { 0.0f, 0.0f } },
        { { +1.0f, +1.0f, -5.0f }, { 1.0f, 1.0f } },
        { { +1.0f, -1.0f, -5.0f }, { 1.0f, 0.0f } },

        { { -1.0f, -1.0f, -3.0f }, { 0.0f, 0.0f } },
        { { -1.0f, +1.0f, -3.0f }, { 0.0f, 1.0f } },
        { { +1.0f, +1.0f, -3.0f }, { 1.0f, 1.0f } },
    };

    Mesh mesh = {};
    if (!load_mesh_gltf(&mesh, "data/meshes/Sponza.gltf")) return 1;

    Vulkan_Graphics_Pipeline pipeline(context.device, globals.window);

    Array <Vulkan_Buffer_And_Memory> per_scene_uniform_buffers;
    if (!context.create_uniform_buffers(per_scene_uniform_buffers, sizeof(Per_Scene_Uniform_Data))) return 1;

    Array <VkDescriptorSet> per_scene_descriptor_sets;
    if (!pipeline.allocate_per_scene_descriptor_sets(per_scene_descriptor_sets, context.images.count)) return 1;
    pipeline.update_per_scene_descriptor_sets(per_scene_uniform_buffers, per_scene_descriptor_sets);
    
    for (int i = 0; i < mesh.num_submeshes; i++) {
        Submesh *submesh = &mesh.submeshes[i];

        submesh->vertex_buffer_size = sizeof(Mesh_Vertex) * submesh->num_vertices;
        submesh->index_buffer_size  = sizeof(u32)         * submesh->num_indices;

        submesh->vertex_buffer = context.create_vertex_buffer(submesh->vertices, submesh->vertex_buffer_size);
        if (!submesh->vertex_buffer.is_valid) return 1;
        
        submesh->index_buffer  = context.create_vertex_buffer(submesh->indices, submesh->index_buffer_size);
        if (!submesh->index_buffer.is_valid) return 1;

        if (!context.create_uniform_buffers(submesh->uniform_buffers, sizeof(Per_Object_Uniform_Data))) return 1;

        if (submesh->material.diffuse_texture_name) {
            const char *extensions[] = {
                "png",
                "jpg",
                "bmp",
            };
            
            char full_path[4096];
            bool full_path_exists = false;
            for (int i = 0; i < ArrayCount(extensions); i++) {
                snprintf(full_path, sizeof(full_path), "data/textures/%s.%s", submesh->material.diffuse_texture_name, extensions[i]);
                if (file_exists(full_path)) {
                    full_path_exists = true;
                    break;
                }
            }

            if (!full_path_exists) {
                logprintf("No file '%s' found in 'data/textures'!\n", submesh->material.diffuse_texture_name);
                return 1;
            }
            
            submesh->texture = context.create_texture(full_path);
        } else {
            submesh->texture = context.create_texture("data/textures/white.png");
        }
        
        if (!pipeline.allocate_descriptor_sets(submesh->descriptor_sets, context.images.count)) return 1;
        pipeline.update_descriptor_sets(submesh->vertex_buffer, submesh->index_buffer, submesh->uniform_buffers, submesh->texture, submesh->descriptor_sets);
    }
    
    if (!pipeline.init(vs, fs, context.get_swap_chain_format(), context.get_depth_format())) return 1;
    
    // Record command buffers
    {
        for (int i = 0; i < command_buffers.count; i++) {            
            if (!vulkan_begin_command_buffer(command_buffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)) {
                return 1;
            }

            vulkan_image_memory_barrier(command_buffers[i], context.images[i], context.get_swap_chain_format(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkClearValue clear_color = {};
            clear_color.color = { 0.2f, 0.5f, 0.8f, 1.0f };

            VkClearValue depth_value = {};
            depth_value.depthStencil.depth = 1.0f;
            depth_value.depthStencil.stencil = 0;

            VkRenderingAttachmentInfoKHR color_attachment = {};
            color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            color_attachment.imageView = context.image_views[i];
            color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
            color_attachment.resolveImageView = VK_NULL_HANDLE;
            color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment.clearValue = clear_color;

            VkRenderingAttachmentInfo depth_attachment = {};
            depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depth_attachment.imageView = context.depth_images[i].view;
            depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
            depth_attachment.resolveImageView = VK_NULL_HANDLE;
            depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth_attachment.clearValue = depth_value;

            VkRenderingInfoKHR rendering_info = {};
            rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
            rendering_info.renderArea.extent.width  = globals.window->width;
            rendering_info.renderArea.extent.height = globals.window->height;
            rendering_info.layerCount = 1;
            rendering_info.colorAttachmentCount = 1;
            rendering_info.pColorAttachments = &color_attachment;
            rendering_info.pDepthAttachment = &depth_attachment;

            vkCmdBeginRendering(command_buffers[i], &rendering_info);
            
            for (int j = 0; j < mesh.num_submeshes; j++) {
                Submesh *submesh = &mesh.submeshes[j];
                vulkan_cmd_bind_pipeline(command_buffers[i], &pipeline, i, per_scene_descriptor_sets, submesh->descriptor_sets);
                vkCmdDraw(command_buffers[i], submesh->num_indices, 1, 0, 0);
            }
            
            vkCmdEndRendering(command_buffers[i]);

            vulkan_image_memory_barrier(command_buffers[i], context.images[i], context.get_swap_chain_format(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            
            if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
                logprintf("Failed to end %d command buffer\n", i);
                return 1;
            }
        }

        logprintf("Vulkan command buffers recorded!\n");
    }

    Camera camera;
    init_camera(&camera, v3(0, GROUND_LEVEL, 0), 0, 0, 0);

    bool should_show_cursor = false;
    if (should_show_cursor) {
        platform_show_and_unlock_cursor();
    } else {
        platform_hide_and_lock_cursor(globals.window);
    }

    float fixed_update_dt = 1.0f / 60.0f;
    u64 last_time = platform_get_time_in_nanoseconds();
    float accumulated_dt = 0.0f;
    while (globals.window->is_open) {
        reset_temporary_storage();

        u64 now_time = platform_get_time_in_nanoseconds();
        u64 dt_ns = now_time - last_time;
        last_time = now_time;

        float dt = (float)((double)dt_ns / 1000000000.0);
        accumulated_dt += dt;
        
        platform_poll_events();

        if (is_key_down(&globals.window->keyboard, KEY_ALT)) {
            if (is_key_pressed(&globals.window->keyboard, KEY_F4)) {
                globals.window->is_open = false;
            } else if (is_key_pressed(&globals.window->keyboard, KEY_ENTER)) {
                platform_window_toggle_fullscreen(globals.window);
            }
        }
        
        if (is_key_pressed(&globals.window->keyboard, KEY_F11)) {
            platform_window_toggle_fullscreen(globals.window);
        }

        if (is_key_pressed(&globals.window->keyboard, KEY_ESCAPE)) {
            should_show_cursor = !should_show_cursor;
            if (should_show_cursor) {
                platform_show_and_unlock_cursor();
            } else {
                platform_hide_and_lock_cursor(globals.window);
            }
        }

        if (should_show_cursor) {
            platform_show_and_unlock_cursor();
        } else {
            platform_hide_and_lock_cursor(globals.window);
        }

        if (!should_show_cursor) {
            update_camera(&camera, CAMERA_TYPE_FPS, dt);
        }

        while (accumulated_dt >= fixed_update_dt) {
            if (!should_show_cursor) {
                fixed_update_camera(&camera, CAMERA_TYPE_FPS, fixed_update_dt);
            }
            accumulated_dt -= fixed_update_dt;
        }
        
        int image_index = command_queue->acquire_next_image();
        if (image_index == -1) return 1;

        Per_Scene_Uniform_Data per_scene_uniforms;
        per_scene_uniforms.projection_matrix = make_perspective((float)globals.window->width / (float)globals.window->height, 45.0f, 0.1f, 1000.0f);
        per_scene_uniforms.projection_matrix._22 *= -1.0f; // Flip the y because Vulkan is left-handed by default
        per_scene_uniforms.view_matrix = get_view_matrix(&camera);

        per_scene_uniforms.projection_matrix = transpose(per_scene_uniforms.projection_matrix);
        per_scene_uniforms.view_matrix = transpose(per_scene_uniforms.view_matrix);

        per_scene_uniform_buffers[image_index].update(context.device, &per_scene_uniforms, sizeof(Per_Scene_Uniform_Data));
        
        Per_Object_Uniform_Data per_object_uniforms;
        per_object_uniforms.world_matrix = make_transformation_matrix(v3(0, -2, -10), v3(0, 0, 0), v3(1, 1, 1));
        per_object_uniforms.world_matrix = transpose(per_object_uniforms.world_matrix);

        for (int i = 0; i < mesh.num_submeshes; i++) {
            Submesh *submesh = &mesh.submeshes[i];
            if (!submesh->uniform_buffers[image_index].update(context.device, &per_object_uniforms, sizeof(Per_Object_Uniform_Data))) return 1;
        }
        
        command_queue->submit_async(command_buffers[image_index]);
        
        command_queue->present(image_index);
    }
    
    return 0;
}
