#include "pch.h"
#include "vulkan_context.h"

int main(int argc, char *argv[]) {
    init_temporary_storage(Megabytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    Platform_Window *window = platform_window_create(0, 0, "Sandbox");

    Vulkan_Context context;
    if (!context.init(window)) return 1;

    int num_images = context.images.count;
    Array <VkCommandBuffer> command_buffers;
    command_buffers.resize(num_images);
    if (!context.create_command_buffers(num_images, command_buffers.data)) return 1;
    Vulkan_Queue *command_queue = &context.command_queue;

    VkRenderPass render_pass = context.create_simple_render_pass();
    if (!render_pass) return 1;
    
    Array <VkFramebuffer> framebuffers;
    if (!context.create_framebuffers(framebuffers, render_pass, window)) return 1;
    
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
        render_pass_begin_info.renderArea.extent.width  = window->width;
        render_pass_begin_info.renderArea.extent.height = window->height;
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_value;

        for (int i = 0; i < command_buffers.count; i++) {            
            if (!vulkan_begin_command_buffer(command_buffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)) {
                return 1;
            }

            render_pass_begin_info.framebuffer = framebuffers[i];

            vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdEndRenderPass(command_buffers[i]);
            
            if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
                logprintf("Failed to end %d command buffer\n", i);
                return 1;
            }
        }

        logprintf("Vulkan command buffers recorded!\n");
    }
    
    while (window->is_open) {
        reset_temporary_storage();
        platform_poll_events();

        if (is_key_down(&window->keyboard, KEY_ALT)) {
            if (is_key_pressed(&window->keyboard, KEY_F4)) {
                window->is_open = false;
            } else if (is_key_pressed(&window->keyboard, KEY_ENTER)) {
                platform_window_toggle_fullscreen(window);
            }
        }
        
        if (is_key_pressed(&window->keyboard, KEY_F11)) {
            platform_window_toggle_fullscreen(window);
        }

        if (is_key_pressed(&window->keyboard, KEY_ESCAPE)) {
            window->is_open = false;
        }

        int image_index = command_queue->acquire_next_image();
        if (image_index == -1) return 1;
        
        command_queue->submit_async(command_buffers[image_index]);
        command_queue->present(image_index);
    }
    
    return 0;
}
