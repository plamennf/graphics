#include "pch.h"
#include "vulkan_context.h"
#include "mesh.h"
#include "camera.h"

struct Uniform_Data {
    Matrix4 wvp;
};

struct Vertex {
    Vector3 position;
    Vector2 uv;
};

Global_Variables globals;

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

    VkRenderPass render_pass = context.create_simple_render_pass();
    if (!render_pass) return 1;
    
    Array <VkFramebuffer> framebuffers;
    if (!context.create_framebuffers(framebuffers, render_pass, globals.window)) return 1;

    VkShaderModule vs = vulkan_create_shader_module_from_binary(context.device, "data/shaders/compiled/test.vert.spv");
    VkShaderModule fs = vulkan_create_shader_module_from_binary(context.device, "data/shaders/compiled/test.frag.spv");
    
    Vertex vertices[] = {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
        { { +1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { { +1.0f, +1.0f, 0.0f }, { 1.0f, 1.0f } },

        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
        { { +1.0f, +1.0f, 0.0f }, { 1.0f, 1.0f } },
        { { -1.0f, +1.0f, 0.0f }, { 0.0f, 1.0f } },
    };

    Mesh mesh = {};
    mesh.vertex_buffer_size = sizeof(vertices);
    mesh.vertex_buffer      = context.create_vertex_buffer(vertices, sizeof(vertices));
    if (!mesh.vertex_buffer.is_valid) return 1;
    mesh.texture            = context.create_texture("data/textures/Building 2.png");
    if (!mesh.texture.is_valid) return 1;
    
    Array <Vulkan_Buffer_And_Memory> uniform_buffers;
    if (!context.create_uniform_buffers(uniform_buffers, sizeof(Uniform_Data))) return 1;
    
    Vulkan_Graphics_Pipeline pipeline;
    if (!pipeline.init(context.device, globals.window, render_pass, vs, fs, &mesh, num_images, uniform_buffers, sizeof(Uniform_Data))) return 1;
    
    // Record command buffers
    {
        VkClearColorValue clear_color = { 0.2f, 0.5f, 0.8f, 1.0f };

        VkClearValue clear_value;
        clear_value.color = clear_color;

        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.renderArea.offset.x = 0;
        render_pass_begin_info.renderArea.offset.y = 0;
        render_pass_begin_info.renderArea.extent.width  = globals.window->width;
        render_pass_begin_info.renderArea.extent.height = globals.window->height;
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_value;

        for (int i = 0; i < command_buffers.count; i++) {            
            if (!vulkan_begin_command_buffer(command_buffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)) {
                return 1;
            }

            render_pass_begin_info.framebuffer = framebuffers[i];

            vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

            vulkan_cmd_bind_pipeline(command_buffers[i], &pipeline, i);

            vkCmdDraw(command_buffers[i], 6, 1, 0, 0);
            
            vkCmdEndRenderPass(command_buffers[i]);
            
            if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
                logprintf("Failed to end %d command buffer\n", i);
                return 1;
            }
        }

        logprintf("Vulkan command buffers recorded!\n");
    }

    Camera camera;
    init_camera(&camera, v3(0, 2, 0), 0, 0, 0);

    bool should_show_cursor = false;
    platform_hide_and_lock_cursor(globals.window);
    
    u64 last_time = platform_get_time_in_nanoseconds();
    while (globals.window->is_open) {
        reset_temporary_storage();

        u64 now_time = platform_get_time_in_nanoseconds();
        u64 dt_ns = now_time - last_time;
        last_time = now_time;

        float dt = (float)((double)dt_ns / 1000000000.0);
        
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
        
        update_camera(&camera, CAMERA_TYPE_NOCLIP, dt);
        fixed_update_camera(&camera, CAMERA_TYPE_NOCLIP, dt);
        
        int image_index = command_queue->acquire_next_image();
        if (image_index == -1) return 1;

        static float foo = 0.0f;
        Matrix4 rotation_matrix = make_z_rotation(foo);
        foo += 0.001f;
        
        Matrix4 projection_matrix = make_perspective((float)globals.window->width / (float)globals.window->height, 45.0f, 0.1f, 1000.0f);
        Matrix4 wvp = rotation_matrix * get_view_matrix(&camera) * projection_matrix;
        wvp = rotation_matrix;
        wvp = transpose(wvp);
        if (!uniform_buffers[image_index].update(context.device, &wvp, sizeof(wvp))) return 1;
        
        command_queue->submit_async(command_buffers[image_index]);
        
        command_queue->present(image_index);
    }
    
    return 0;
}
