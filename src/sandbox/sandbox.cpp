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

    VkCommandBuffer command_buffers[NUM_FRAMES_IN_FLIGHT];
    if (!context.create_command_buffers(NUM_FRAMES_IN_FLIGHT, command_buffers)) return 1;

    VkShaderModule vs = vulkan_create_shader_module_from_binary(context.device, "data/shaders/compiled/test.vert.spv");
    VkShaderModule fs = vulkan_create_shader_module_from_binary(context.device, "data/shaders/compiled/test.frag.spv");
    
    Mesh mesh = {};
    if (!load_mesh_gltf(&mesh, "data/meshes/Sponza.gltf")) return 1;

    Vulkan_Graphics_Pipeline pipeline;
    if (!pipeline.init(context.device, vs, fs, context.get_swap_chain_format(), context.get_depth_format(), mesh.num_submeshes)) return 1;
    
    Vulkan_Buffer per_scene_uniform_buffers[NUM_FRAMES_IN_FLIGHT];
    if (!context.create_shader_device_address_buffers(per_scene_uniform_buffers, sizeof(Per_Scene_Uniform_Data))) return 1;

    Vulkan_Buffer per_object_uniform_buffers[NUM_FRAMES_IN_FLIGHT];
    if (!context.create_shader_device_address_buffers(per_object_uniform_buffers, sizeof(Per_Object_Uniform_Data))) return 1;
    
    for (int i = 0; i < mesh.num_submeshes; i++) {
        Submesh *submesh = &mesh.submeshes[i];

        submesh->vertex_buffer_size = sizeof(Mesh_Vertex) * submesh->num_vertices;
        submesh->index_buffer_size  = sizeof(u32)         * submesh->num_indices;

        submesh->vertex_index_buffer = context.create_vertex_index_buffer(submesh->vertex_buffer_size, submesh->vertices, submesh->index_buffer_size, submesh->indices);
        if (!submesh->vertex_index_buffer.is_valid) return 1;

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

        pipeline.update_descriptor_set(context.device, pipeline.descriptor_sets[i], submesh->texture);
    }

    Camera camera;
    init_camera(&camera, v3(0, GROUND_LEVEL, 0), 0, 0, 0);

    bool should_show_cursor = false;
    if (should_show_cursor) {
        platform_show_and_unlock_cursor();
    } else {
        platform_hide_and_lock_cursor(globals.window);
    }

    int previous_window_width  = globals.window->width;
    int previous_window_height = globals.window->height;

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

        if (globals.window->width  != previous_window_width ||
            globals.window->height != previous_window_height) {
            if (globals.window->width != 0 && globals.window->height != 0) {
                context.update_swap_chain = true;
                previous_window_width  = globals.window->width;
                previous_window_height = globals.window->height;
            }
        }

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

        int should_render = context.begin_frame();
        if (should_render == -1) return false;
        if (should_render == 0) {
            if (!context.end_frame(VK_NULL_HANDLE, globals.window)) {
                return 1;
            }
            continue;
        }

        VkCommandBuffer cb = command_buffers[context.frame_index];
        if (vkResetCommandBuffer(cb, 0) != VK_SUCCESS) {
            logprintf("Failed to reset vulkan command buffer!\n");
            return 1;
        }

        if (!vulkan_begin_command_buffer(cb, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) return 1;

        VkImageMemoryBarrier2 output_barriers[2] = {};

        output_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        output_barriers[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        output_barriers[0].srcAccessMask = 0;
        output_barriers[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        output_barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        output_barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        output_barriers[0].newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        output_barriers[0].image     = context.images[context.image_index];
        output_barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        output_barriers[0].subresourceRange.levelCount = 1;
        output_barriers[0].subresourceRange.layerCount = 1;

        output_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        output_barriers[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        output_barriers[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        output_barriers[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        output_barriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        output_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        output_barriers[1].newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        output_barriers[1].image     = context.depth_images[context.image_index].image;
        output_barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;// | VK_IMAGE_ASPECT_STENCIL_BIT;
        output_barriers[1].subresourceRange.levelCount = 1;
        output_barriers[1].subresourceRange.layerCount = 1;

        VkDependencyInfo barrier_dependency_info = {};
        barrier_dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        barrier_dependency_info.imageMemoryBarrierCount = ArrayCount(output_barriers);
        barrier_dependency_info.pImageMemoryBarriers = output_barriers;

        vkCmdPipelineBarrier2(cb, &barrier_dependency_info);
        
        VkClearValue clear_color = {};
        clear_color.color = { 0.2f, 0.5f, 0.8f, 1.0f };

        VkClearValue depth_value = {};
        depth_value.depthStencil.depth = 1.0f;
        depth_value.depthStencil.stencil = 0;

        VkRenderingAttachmentInfoKHR color_attachment = {};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        color_attachment.imageView = context.image_views[context.image_index];
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        color_attachment.resolveImageView = VK_NULL_HANDLE;
        color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue = clear_color;

        VkRenderingAttachmentInfo depth_attachment = {};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = context.depth_images[context.image_index].view;
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

        vkCmdBeginRendering(cb, &rendering_info);

        VkViewport viewport = {};
        viewport.width    = (float)globals.window->width;
        viewport.height   = (float)globals.window->height;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.extent.width  = globals.window->width;
        scissor.extent.height = globals.window->height;
        
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &scissor);
        
        Per_Scene_Uniform_Data per_scene_uniforms;
        per_scene_uniforms.projection_matrix = make_perspective((float)globals.window->width / (float)globals.window->height, 90.0f, 0.1f, 2000.0f);
        per_scene_uniforms.projection_matrix._22 *= -1.0f; // Flip the y because Vulkan is left-handed by default
        per_scene_uniforms.view_matrix = get_view_matrix(&camera);

        per_scene_uniforms.projection_matrix = transpose(per_scene_uniforms.projection_matrix);
        per_scene_uniforms.view_matrix = transpose(per_scene_uniforms.view_matrix);

        per_scene_uniform_buffers[context.frame_index].update(context.allocator, &per_scene_uniforms, sizeof(Per_Scene_Uniform_Data));

        VkDeviceAddress push_constants_device_addresses[] = {
            per_scene_uniform_buffers[context.frame_index].device_address,
            per_object_uniform_buffers[context.frame_index].device_address,
        };
        
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        vkCmdPushConstants(cb, pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants_device_addresses), push_constants_device_addresses);
        
        Per_Object_Uniform_Data per_object_uniforms;
        per_object_uniforms.world_matrix = make_transformation_matrix(v3(0, -2, -10), v3(0, 0, 0), v3(1, 1, 1));
        per_object_uniforms.world_matrix = transpose(per_object_uniforms.world_matrix);

        per_object_uniform_buffers[context.frame_index].update(context.allocator, &per_object_uniforms, sizeof(Per_Object_Uniform_Data));
        
        for (int i = 0; i < mesh.num_submeshes; i++) {
            Submesh *submesh = &mesh.submeshes[i];

            VkDeviceSize offset = 0;
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline_layout, 0, 1, &pipeline.descriptor_sets[i], 0, NULL);
            vkCmdBindVertexBuffers(cb, 0, 1, &submesh->vertex_index_buffer.buffer, &offset);
            vkCmdBindIndexBuffer(cb, submesh->vertex_index_buffer.buffer, submesh->vertex_buffer_size, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cb, submesh->num_indices, 1, 0, 0, 0);
        }

        vkCmdEndRendering(cb);

        VkImageMemoryBarrier2 barrier_present = {};
        barrier_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier_present.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_present.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier_present.dstAccessMask = 0;
        barrier_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier_present.image = context.images[context.image_index];
        barrier_present.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier_present.subresourceRange.levelCount = 1;
        barrier_present.subresourceRange.layerCount = 1;

        VkDependencyInfo barrier_present_dependency_info = {};
        barrier_present_dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        barrier_present_dependency_info.imageMemoryBarrierCount = 1;
        barrier_present_dependency_info.pImageMemoryBarriers = &barrier_present;

        vkCmdPipelineBarrier2(cb, &barrier_present_dependency_info);

        vkEndCommandBuffer(cb);
        
        if (!context.end_frame(cb, globals.window)) {
            return 1;
        }
    }
    
    return 0;
}
