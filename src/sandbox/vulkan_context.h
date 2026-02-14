#pragma once

struct Mesh;

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
    VkFormat depth_format;
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

struct Vulkan_Buffer_And_Memory {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize allocation_size = 0;
    bool is_valid = false;

    bool update(VkDevice device, void *data, u32 size);
    void destroy(VkDevice device);
};

struct Vulkan_Graphics_Pipeline {
    VkDevice device = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    Array <VkDescriptorSet> descriptor_sets;
    
    bool init(VkDevice device, Platform_Window *window, VkRenderPass render_pass, VkShaderModule vs, VkShaderModule fs, Mesh *mesh, int num_images, Array <Vulkan_Buffer_And_Memory> &uniform_buffers, VkDeviceSize uniform_buffer_size);
    bool create_descriptor_sets(Mesh *mesh, int num_images, Array <Vulkan_Buffer_And_Memory> &uniform_buffers, VkDeviceSize uniform_buffer_size);
};

struct Vulkan_Texture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    bool is_valid = false;
    
    void destroy(VkDevice device);
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
    Array <Vulkan_Texture> depth_images;
    VkCommandPool command_buffer_pool;
    Vulkan_Queue command_queue;
    VkCommandBuffer copy_command_buffer;
    
    bool init(Platform_Window *window);

    bool create_command_buffers(int num_command_buffers, VkCommandBuffer *command_buffers);
    VkRenderPass create_simple_render_pass();
    bool create_framebuffers(Array <VkFramebuffer> &framebuffers, VkRenderPass render_pass, Platform_Window *window);
    Vulkan_Buffer_And_Memory create_vertex_buffer(void *data, u32 size);
    bool create_uniform_buffers(Array <Vulkan_Buffer_And_Memory> &buffers, u32 size);

    Vulkan_Texture create_texture(String filepath);
    Vulkan_Texture create_texture_image_from_data(void *data, int width, int height, VkFormat format);
    
private:
    bool create_instance();
    bool create_debug_callback();
    bool create_surface(Platform_Window *window);
    bool create_device();
    bool create_swap_chain();
    bool create_command_buffer_pool();

    bool create_depth_resources(Platform_Window *window);
    
    Vulkan_Buffer_And_Memory create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    bool copy_buffer(VkBuffer destination, VkBuffer source, VkDeviceSize size);

    Vulkan_Buffer_And_Memory create_uniform_buffer(VkDeviceSize size);

    Vulkan_Texture create_image(int width, int height, VkFormat format, VkImageUsageFlags usage_flags, VkMemoryPropertyFlagBits property_flags);
    bool update_texture_image(Vulkan_Texture *texture, int width, int height, VkFormat format, void *data);

    bool transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);

    bool copy_buffer_to_buffer(VkBuffer dst, VkBuffer src, VkDeviceSize size);
    bool copy_buffer_to_image(VkImage dst, VkBuffer src, int width, int height);
    
    void submit_copy_command();
    
    u32 get_memory_type_index(u32 memory_type_bits_mask, VkMemoryPropertyFlags required_memory_property_flags);
};

bool vulkan_begin_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferUsageFlags usage_flags);
VkSemaphore vulkan_create_semaphore(VkDevice device);
VkShaderModule vulkan_create_shader_module_from_binary(VkDevice device, String filename);
bool vulkan_image_memory_barrier(VkCommandBuffer buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
VkSampler vulkan_create_texture_sampler(VkDevice device, VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode address_mode);

void vulkan_cmd_bind_pipeline(VkCommandBuffer buffer, Vulkan_Graphics_Pipeline *pipeline, int image_index);
