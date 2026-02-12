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
};

struct Vulkan_Physical_Devices {
    Array <Vulkan_Physical_Device> devices;
    int device_index = -1;
    
    bool init(VkInstance instance, VkSurfaceKHR surface);
    u32 select_device(VkQueueFlags required_queue_type, bool supports_present);
    Vulkan_Physical_Device *get_selected();
};

struct Vulkan_Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Vulkan_Physical_Devices physical_devices;
    u32 queue_family = 0;
    
    bool init(Platform_Window *window);
    
private:
    bool create_instance();
    bool create_debug_callback();
    bool create_surface(Platform_Window *window);
};
