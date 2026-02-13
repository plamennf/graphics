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

    /*
    VkRenderPass render_pass = context.create_simple_render_pass();
    if (!render_pass) return 1;
    
    Array <VkFramebuffer> framebuffers;
    if (!context.create_framebuffer(render_pass)) return 1;
    */
    
    // Record command buffers
    {
        VkClearColorValue clear_color = { 0.2f, 0.5f, 0.8f, 1.0f };

        /*
        VkClearValue clear_value;
        clear_value.color = clear_color;

        VkRenderPassBeginInfo render_pass_begin_info = {};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.renderArea.offset.x = 0;
        render_pass_begin_info.renderArea.offset.y = 0;
        render_pass_begin_info.renderArea.extent.width  = window->width;
        render_pass_begin_info.renderArea.extent.height = window->height;
        render_pass_begin_info.clearValueColor = 1;
        render_pass_begin_info.pClearValues = &clear_value;
        */
        
        VkImageSubresourceRange image_range = {};
        image_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        image_range.baseMipLevel   = 0;
        image_range.levelCount     = 1;
        image_range.baseArrayLayer = 0;
        image_range.layerCount     = 1;

        for (int i = 0; i < command_buffers.count; i++) {
            VkImageMemoryBarrier present_to_clear_barrier = {};
            present_to_clear_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            present_to_clear_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            present_to_clear_barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            present_to_clear_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            present_to_clear_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            present_to_clear_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            present_to_clear_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            present_to_clear_barrier.image = context.images[i];
            present_to_clear_barrier.subresourceRange = image_range;

            VkImageMemoryBarrier clear_to_present_barrier = {};
            clear_to_present_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            clear_to_present_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            clear_to_present_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            clear_to_present_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            clear_to_present_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            clear_to_present_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            clear_to_present_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            clear_to_present_barrier.image = context.images[i];
            clear_to_present_barrier.subresourceRange = image_range;
            
            if (!vulkan_begin_command_buffer(command_buffers[i], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)) {
                return 1;
            }

            vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &present_to_clear_barrier);

            vkCmdClearColorImage(command_buffers[i], context.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image_range);

            vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &clear_to_present_barrier);
            
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
