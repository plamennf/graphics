#pragma once

struct Vulkan_Physical_Device {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    Array <VkQueueFamilyProperties> queue_family_properties;
    Array <VkBool32> queue_supports_present;
    Array <VkSurfaceFormatKHR> surface_formats;
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VkPhysicalDeviceMemoryProperties memory_properties;
    Array <VkPresentModeKHR> present_modes;
    VkPhysicalDeviceFeatures features;
};

struct Vulkan_Physical_Devices {
    Array <Vulkan_Physical_Device> devices;
    int device_index = -1;
    
    bool init(VkInstance instance, VkSurfaceKHR surface);
    u32 select_device(VkQueueFlags required_queue_type, bool supports_present);
    Vulkan_Physical_Device *get_selected();
};

struct Vulkan_Queue {
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkSemaphore render_complete_semaphore = VK_NULL_HANDLE;
    VkSemaphore present_complete_semaphore = VK_NULL_HANDLE;
    
    bool init(VkDevice device, VkSwapchainKHR swap_chain, u32 queue_family, u32 queue_index);

    int acquire_next_image();
    bool submit_sync(VkCommandBuffer command_buffer);
    bool submit_async(VkCommandBuffer command_buffer);
    bool present(int image_index);
    void wait_idle();
};

struct Vulkan_Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Vulkan_Physical_Devices physical_devices;
    Vulkan_Physical_Device *selected_physical_device;
    u32 queue_family = 0;
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR swap_chain_surface_format;
    Array <VkImage> images;
    Array <VkImageView> image_views;
    VkCommandPool command_buffer_pool;
    Vulkan_Queue command_queue;
    
    bool init(Platform_Window *window);

    bool create_command_buffers(int num_command_buffers, VkCommandBuffer *command_buffers);
    VkRenderPass create_simple_render_pass();
    bool create_framebuffers(Array <VkFramebuffer> &framebuffers, VkRenderPass render_pass, Platform_Window *window);
    
private:
    bool create_instance();
    bool create_debug_callback();
    bool create_surface(Platform_Window *window);
    bool create_device();
    bool create_swap_chain();
    bool create_command_buffer_pool();
};

bool vulkan_begin_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferUsageFlags usage_flags);
VkSemaphore vulkan_create_semaphore(VkDevice device);
