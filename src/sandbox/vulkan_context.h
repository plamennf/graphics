#pragma once

#include <vma/vk_mem_alloc.h>

struct Mesh_Vertex {
    Vector3 position;
    Vector2 uv;
    Vector3 normal;
    Vector3 tangent;
    Vector3 bitangent;
};

const int NUM_FRAMES_IN_FLIGHT = 2;

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

    u32 graphics_and_present_queue_family_index = 0;
};

struct Vulkan_Physical_Devices {
    Array <Vulkan_Physical_Device> devices;
    int device_index = -1;
    
    bool init(VkInstance instance, VkSurfaceKHR surface);
    void select_device(VkQueueFlags required_queue_type, bool supports_present);
    Vulkan_Physical_Device *get_selected();
};

struct Vulkan_Queue {
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    
    bool init(VkDevice device, VkSwapchainKHR swap_chain, u32 queue_family, u32 queue_index);

    int acquire_next_image();
    bool submit_sync(VkCommandBuffer command_buffer);
    bool submit_async(VkCommandBuffer command_buffer);
    bool present(int image_index);
    void wait_idle();
};

struct Vulkan_Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = {};
    VkDeviceSize allocation_size = 0;
    VkDeviceSize size = 0;
    bool is_valid = false;

    // Shader device address buffers
    void *mapped_data = NULL;
    VkDeviceAddress device_address = {};

    bool update(VmaAllocator allocator, void *data, VkDeviceSize size);
    void destroy(VkDevice device);
};

struct Vulkan_Texture {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = {};
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    bool is_valid = false;
    
    void destroy(VkDevice device);
};

struct Vulkan_Graphics_Pipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    Array <VkDescriptorSet> descriptor_sets; // For now we create a descriptor set for every submesh. Should change that in the future.
    
    bool init(VkDevice device, VkShaderModule vs, VkShaderModule fs, VkFormat color_format, VkFormat depth_format, int num_submeshes);
    void update_descriptor_set(VkDevice device, VkDescriptorSet set, Vulkan_Texture texture);
};

struct Vulkan_Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Vulkan_Physical_Devices physical_devices;
    Vulkan_Physical_Device *selected_physical_device;
    u32 queue_family = 0;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR swap_chain_surface_format;
    Array <VkImage> images;
    Array <VkImageView> image_views;
    Array <Vulkan_Texture> depth_images;
    VkCommandPool command_buffer_pool;

    VkQueue graphics_and_present_queue = VK_NULL_HANDLE;
    VkFence fences[NUM_FRAMES_IN_FLIGHT];
    VkSemaphore present_complete_semaphores[NUM_FRAMES_IN_FLIGHT];
    Array <VkSemaphore> render_complete_semaphores;

    int frame_index = 0; // Used for resources that in flight
    int image_index = 0; // Used for resources that depend on the swap chain buffers
    
    VkCommandBuffer copy_command_buffer;

    bool init(Platform_Window *window);

    bool begin_frame();
    bool end_frame(VkCommandBuffer cb);

    bool create_command_buffers(int num_commands_buffers, VkCommandBuffer *command_buffers);
    Vulkan_Buffer create_vertex_index_buffer(VkDeviceSize vertex_buffer_size, void *vertex_buffer_data, VkDeviceSize index_buffer_size, void *index_buffer_data);
    bool create_shader_device_address_buffers(Vulkan_Buffer buffers[NUM_FRAMES_IN_FLIGHT], VkDeviceSize size);

    Vulkan_Texture create_texture(String filepath);
    Vulkan_Texture create_texture_image_from_data(void *data, int width, int height, VkFormat format);

    inline VkFormat get_swap_chain_format() const { return swap_chain_surface_format.format; }
    inline VkFormat get_depth_format() const { Assert(selected_physical_device); return selected_physical_device->depth_format; }
    
private:
    bool create_instance();
    bool create_debug_callback();
    bool create_surface(Platform_Window *window);
    bool create_device();
    bool init_vma();
    bool create_swap_chain();
    bool create_command_buffer_pool();
    bool create_synchronization_objects();
    
    bool create_depth_resources(Platform_Window *window);

    Vulkan_Buffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    bool copy_buffer(VkBuffer destination, VkBuffer source, VkDeviceSize size);
    
    Vulkan_Texture create_image(int width, int height, VkFormat format, VkImageUsageFlags usage_flags, VkMemoryPropertyFlagBits property_flags);
    bool update_texture_image(Vulkan_Texture *texture, int width, int height, VkFormat format, void *data);

    bool transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);

    bool copy_buffer_to_buffer(VkBuffer dst, VkBuffer src, VkDeviceSize size);
    bool copy_buffer_to_image(VkImage dst, VkBuffer src, int width, int height);
    
    u32 get_memory_type_index(u32 memory_type_bits_mask, VkMemoryPropertyFlags required_memory_property_flags);
};

bool vulkan_begin_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferUsageFlags usage_flags);
VkSemaphore vulkan_create_semaphore(VkDevice device);
VkShaderModule vulkan_create_shader_module_from_binary(VkDevice device, String filename);
bool vulkan_image_memory_barrier(VkCommandBuffer buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
VkSampler vulkan_create_texture_sampler(VkDevice device, VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode address_mode);

void vulkan_cmd_bind_pipeline(VkCommandBuffer command_buffer, Vulkan_Graphics_Pipeline *pipeline, int image_index, Array <VkDescriptorSet> const &per_scene_descriptor_sets, Array <VkDescriptorSet> const &descriptor_sets);
