#include "pch.h"

#define VMA_IMPLEMENTATION
#include "vulkan_context.h"
#include "mesh.h"

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdio.h>

static bool has_stencil_component(VkFormat format) {
	return ((format == VK_FORMAT_D32_SFLOAT_S8_UINT) || 
		    (format == VK_FORMAT_D24_UNORM_S8_UINT));
}

static VkFormat find_supported_format(VkPhysicalDevice device, Array <VkFormat> const &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (int i = 0; i < candidates.count; i++) {
        VkFormat format = candidates[i];
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(device, format, &properties);

        if ((tiling == VK_IMAGE_TILING_LINEAR) &&
            (properties.linearTilingFeatures & features) == features) {
            return format;
        } else if ((tiling == VK_IMAGE_TILING_OPTIMAL) &&
                   (properties.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    Assert(!"Failed to find support vulkan format!");
    return (VkFormat)0;
}

static VkFormat find_depth_format(VkPhysicalDevice physical_device) {
    Array <VkFormat> candidates;
    candidates.use_temporary_storage = true;
    candidates.add(VK_FORMAT_D32_SFLOAT);
    candidates.add(VK_FORMAT_D32_SFLOAT_S8_UINT);
    candidates.add(VK_FORMAT_D24_UNORM_S8_UINT);

    return find_supported_format(physical_device, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static int get_bpp(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_UNORM:
            return 1;
            
        case VK_FORMAT_R16_SFLOAT:
            return 2;

        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;

        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return 4 * sizeof(uint16_t);

        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4 * sizeof(float);
    }

    Assert(!"Unknown vulkan format");
    return 0;
}

static const char *get_debug_message_severity_string(VkDebugUtilsMessageSeverityFlagBitsEXT severity) {
	switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
            return "Verbose";
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
            return "Info";
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
            return "Warning";
        } break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
            return "Error";
        } break;

        default: {
            logprintf("Invalid severity code %d\n", severity);
        } break;
	}

	return "NO SUCH SEVERITY!";
}


static const char *get_debug_message_type_string(VkDebugUtilsMessageTypeFlagsEXT type) {
	switch (type) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: {
            return "General";
        } break;

        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: {
            return "Validation";
        } break;

        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: {
            return "Performance";
        } break;

#ifdef PLATFORM_WINDOWS // doesn't work on my Linux for some reason
        case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT: {
            return "Device address binding";
        } break;
#endif
            
        default: {
            logprintf("Invalid type code %d\n", type);
        } break;
	}

	return "NO SUCH TYPE!";
}

static void print_image_usage_flags(VkImageUsageFlags flags, const char *prefix = "") {
    if (flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        logprintf("%sImage usage transfer src is supported\n", prefix);
    }

    if (flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        logprintf("%sImage usage transfer dest is supported\n", prefix);
    }

    if (flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
        logprintf("%sImage usage sampled is supported\n", prefix);
    }

    if (flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
        logprintf("%sImage usage color attachment is supported\n", prefix);
    }

    if (flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        logprintf("%sImage usage depth stencil attachment is supported\n", prefix);
    }

    if (flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
        logprintf("%sImage usage transient attachment is supported\n", prefix);
    }

    if (flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
        logprintf("%sImage usage input attachment is supported\n", prefix);
    }
}

static void print_memory_property(VkMemoryPropertyFlags property_flags, const char *prefix = "") {
    if (property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
        logprintf("%sDEVICE LOCAL ", prefix);
    }

    if (property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        logprintf("%sHOST VISIBLE ", prefix);
    }

    if (property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        logprintf("%sHOST COHERENT ", prefix);
    }

    if (property_flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
        logprintf("%sHOST CACHED ", prefix);
    }

    if (property_flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
        logprintf("%sLAZILY ALLOCATED ", prefix);
    }

    if (property_flags & VK_MEMORY_PROPERTY_PROTECTED_BIT) {
        logprintf("%sPROTECTED ", prefix);
    }
}

static VkBool32 VKAPI_PTR vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
    logprintf("Debug callback: %s\n", callback_data->pMessage);
    logprintf("  Severity: %s\n", get_debug_message_severity_string(message_severity));
    logprintf("  Type: %s\n", get_debug_message_type_string(message_type));
    logprintf("  Objects:");

    for (u32 i = 0; i < callback_data->objectCount; i++) {
        logprintf("%llx ", callback_data->pObjects[i].objectHandle);
    }

    logprintf("\n");

    return VK_FALSE;
}

bool Vulkan_Context::init(Platform_Window *window) {
    if (!create_instance()) return false;
    if (!create_debug_callback()) return false;
    if (!create_surface(window)) return false;
    if (!physical_devices.init(instance, surface)) return false;
    physical_devices.select_device(VK_QUEUE_GRAPHICS_BIT, true);
    selected_physical_device = physical_devices.get_selected();
    if (!selected_physical_device) return false;
    if (!create_device()) return false;
    if (!init_vma()) return false;
    if (!create_swap_chain()) return false;
    if (!create_command_buffer_pool()) return false;
    //if (!command_queue.init(device, swap_chain, queue_family, 0)) return false;
    if (!create_command_buffers(1, &copy_command_buffer)) return false;
    if (!create_depth_resources(window)) return false;
    if (!create_synchronization_objects()) return false;
    
    return true;
}

bool Vulkan_Context::begin_frame() {
    CHECK_VK_RESULT(vkWaitForFences(device, 1, &fences[frame_index], true, UINT64_MAX), "Faileld to wait for fence");
    CHECK_VK_RESULT(vkResetFences(device, 1, &fences[frame_index]), "Failed to reset fence");

    if (vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, present_complete_semaphores[frame_index], VK_NULL_HANDLE, (u32 *)&image_index) != VK_SUCCESS) {
        // TODO: Recreate swap chain
    }

    return true;
}

bool Vulkan_Context::end_frame(VkCommandBuffer cb) {
    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &present_complete_semaphores[frame_index];
    submit_info.pWaitDstStageMask = &wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cb;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphores[image_index];

    CHECK_VK_RESULT(vkQueueSubmit(graphics_and_present_queue, 1, &submit_info, fences[frame_index]), "Failed to submit to graphics and present queue");

    frame_index = (frame_index + 1) & NUM_FRAMES_IN_FLIGHT;

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphores[image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swap_chain;
    present_info.pImageIndices = (u32 *)&image_index;

    if (vkQueuePresentKHR(graphics_and_present_queue, &present_info) != VK_SUCCESS) {
        // TODO: Recreate the swap chain
    }

    return true;
}

bool Vulkan_Context::create_instance() {    
    Array <const char *> validation_layers;
    validation_layers.use_temporary_storage = true;
    validation_layers.add("VK_LAYER_KHRONOS_validation");

    Array <const char *> extensions;
    extensions.use_temporary_storage = true;
    extensions.add(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef PLATFORM_WINDOWS
    extensions.add("VK_KHR_win32_surface");
#endif
    extensions.add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo app_info  = {};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "Graphics Application";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.pEngineName        = "No Engine";
    app_info.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info    = {};
    create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo        = &app_info;
    create_info.enabledLayerCount       = (u32)validation_layers.count;
    create_info.ppEnabledLayerNames     = validation_layers.data;
    create_info.enabledExtensionCount   = (u32)extensions.count;
    create_info.ppEnabledExtensionNames = extensions.data;

    CHECK_VK_RESULT(vkCreateInstance(&create_info, NULL, &instance), "Failed to create vulkan instance");

    logprintf("Vulkan instance created!\n");

    return true;
}

bool Vulkan_Context::create_debug_callback() {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
    create_info.messageType     = (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    create_info.pfnUserCallback = vulkan_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = VK_NULL_HANDLE;
    vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (!vkCreateDebugUtilsMessengerEXT) {
        logprintf("Failed to find address of vkCreateDebugUtilsMessengerEXT\n");
        return false;
    }
    
    CHECK_VK_RESULT(vkCreateDebugUtilsMessengerEXT(instance, &create_info, NULL, &debug_messenger), "Failed to create the debug utils messenger");

    logprintf("Vulkan debug utils messenger created!\n");
    
    return true;
}

bool Vulkan_Context::create_surface(Platform_Window *window) {
#ifdef PLATFORM_WINDOWS
    VkWin32SurfaceCreateInfoKHR create_info = {};
    create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = GetModuleHandleW(NULL);
    create_info.hwnd      = (HWND)platform_window_get_native(window);

    CHECK_VK_RESULT(vkCreateWin32SurfaceKHR(instance, &create_info, NULL, &surface), "Failed to create surface");
#endif

    logprintf("Vulkan surface created!");
    
    return true;
}

bool Vulkan_Context::create_device() {
    float queue_priorities[] = { 1.0f };

    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount       = 1;
    queue_create_info.pQueuePriorities = queue_priorities;

    Array <const char *> device_extensions;
    device_extensions.use_temporary_storage = true;
    device_extensions.add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.add(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
    
    if (selected_physical_device->features.geometryShader == VK_FALSE) {
        logprintf("The geometry shader is not supported!\n");
        return false;
    }

    if (selected_physical_device->features.tessellationShader == VK_FALSE) {
        logprintf("The tessellation shader is not supported!\n");
        return false;
    }

    VkPhysicalDeviceVulkan12Features enabled_vk12_features = {};
    enabled_vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabled_vk12_features.descriptorIndexing = true;
    enabled_vk12_features.shaderSampledImageArrayNonUniformIndexing = true;
    enabled_vk12_features.descriptorBindingVariableDescriptorCount = true;
    enabled_vk12_features.runtimeDescriptorArray = true;
    enabled_vk12_features.bufferDeviceAddress = true;
    
    VkPhysicalDeviceVulkan13Features enabled_vk13_features = {};
    enabled_vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enabled_vk13_features.pNext = &enabled_vk12_features;
    enabled_vk13_features.synchronization2 = true;
    enabled_vk13_features.dynamicRendering = true;
    
    VkPhysicalDeviceFeatures enabled_vk10_features = {};
    enabled_vk10_features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext                   = &enabled_vk13_features;
    device_create_info.queueCreateInfoCount    = 1;
    device_create_info.pQueueCreateInfos       = &queue_create_info;
    device_create_info.enabledExtensionCount   = device_extensions.count;
    device_create_info.ppEnabledExtensionNames = device_extensions.data;
    device_create_info.pEnabledFeatures        = &enabled_vk10_features;

    CHECK_VK_RESULT(vkCreateDevice(selected_physical_device->device, &device_create_info, NULL, &device), "Failed to create a vulkan logical device");

    logprintf("\nVulkan logical device created!\n");

    {
        u32 queue_family_index = selected_physical_device->graphics_and_present_queue_family_index;
        vkGetDeviceQueue(device, queue_family_index, 0, &graphics_and_present_queue);
    }
    
    return true;
}

bool Vulkan_Context::init_vma() {
    VmaVulkanFunctions vk_functions = {};
    vk_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vk_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vk_functions.vkCreateImage = vkCreateImage;

    VmaAllocatorCreateInfo allocator_create_info = {};
    allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_create_info.physicalDevice = selected_physical_device->device;
    allocator_create_info.device = device;
    allocator_create_info.pVulkanFunctions = &vk_functions;
    allocator_create_info.instance = instance;

    CHECK_VK_RESULT(vmaCreateAllocator(&allocator_create_info, &allocator), "Failed to create vma allocator");

    logprintf("VMA Allocator initted!\n");
    
    return true;
}

bool Vulkan_Physical_Devices::init(VkInstance instance, VkSurfaceKHR surface) {
    u32 num_devices = 0;

    CHECK_VK_RESULT(vkEnumeratePhysicalDevices(instance, &num_devices, NULL), "Failed to enumerate physical devices");

    logprintf("\nNum physical devices: %d\n", num_devices);

    devices.resize(num_devices);
    for (int i = 0; i < devices.count; i++) {
        devices[i] = Vulkan_Physical_Device();
    }
    
    auto physical_devices = TAllocArray(VkPhysicalDevice, num_devices);

    CHECK_VK_RESULT(vkEnumeratePhysicalDevices(instance, &num_devices, physical_devices), "Failed to enumerate physical devices");

    for (u32 i = 0; i < num_devices; i++) {
        VkPhysicalDevice physical_device = physical_devices[i];
        Vulkan_Physical_Device *device   = &devices[i];

        device->device = physical_device;
        vkGetPhysicalDeviceProperties(physical_device, &device->properties);

        logprintf("  %s:\n", device->properties.deviceName);

        u32 num_queue_families;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, NULL);
        logprintf("    Num of queue families: %d\n", num_queue_families);

        device->queue_family_properties.resize(num_queue_families);
        device->queue_supports_present.resize(num_queue_families);

        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, device->queue_family_properties.data);

        for (u32 j = 0; j < num_queue_families; j++) {
            VkQueueFamilyProperties properties = device->queue_family_properties[j];

            logprintf("    Family: %d, Num Queues: %d\n", j, properties.queueCount);

            VkQueueFlags flags = properties.queueFlags;
            logprintf("    GFX: %s, Compute %s, Transfer %s, Sparse binding %s\n",
                      (flags & VK_QUEUE_GRAPHICS_BIT) ? "Yes" : "No",
                      (flags & VK_QUEUE_COMPUTE_BIT) ? "Yes" : "No",
                      (flags & VK_QUEUE_TRANSFER_BIT) ? "Yes" : "No",
                      (flags & VK_QUEUE_SPARSE_BINDING_BIT) ? "Yes" : "No");

            CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, j, surface, &device->queue_supports_present[j]), "Failed to get physical device surface support");
        }

        u32 num_formats = 0;
        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_formats, NULL), "Failed to get num of surface formats supported by physical device");
        Assert(num_formats > 0);

        device->surface_formats.resize(num_formats);

        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &num_formats, device->surface_formats.data), "Failed to get surface formats supported by physical device");

        for (u32 j = 0; j < num_formats; j++) {
            VkSurfaceFormatKHR surface_format = device->surface_formats[j];
            logprintf("    Format: %x, Color space: %x\n", surface_format.format, surface_format.colorSpace);
        }

        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &device->surface_capabilities), "Failed to get physical device surface capabilities");

        print_image_usage_flags(device->surface_capabilities.supportedUsageFlags, "      ");

        u32 num_present_modes;
        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_present_modes, NULL), "Failed to get num of present modes supported by the physical device and the surface");
        Assert(num_present_modes > 0);

        device->present_modes.resize(num_present_modes);
        CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &num_present_modes, device->present_modes.data), "Failed to get present modes supported by the physical device and the surface");

        logprintf("    Number of present modes: %d\n", num_present_modes);

        vkGetPhysicalDeviceMemoryProperties(physical_device, &device->memory_properties);

        logprintf("    Number of memory types: %d\n", device->memory_properties.memoryTypeCount);
        for (u32 j = 0; j < device->memory_properties.memoryTypeCount; j++) {
            logprintf("      %d: flags: %x, heap: %d ", j,
                      device->memory_properties.memoryTypes[j].propertyFlags,
                      device->memory_properties.memoryTypes[j].heapIndex);

            print_memory_property(device->memory_properties.memoryTypes[j].propertyFlags, "      ");
            logprintf("\n");
        }

        logprintf("    Number of heap types: %d\n\n", device->memory_properties.memoryHeapCount);

        vkGetPhysicalDeviceFeatures(physical_device, &device->features);

        device->depth_format = find_depth_format(physical_device);
    }

    return true;
}

void Vulkan_Physical_Devices::select_device(VkQueueFlags required_queue_type, bool supports_present) {
    for (int i = 0; i < devices.count; i++) {
        Vulkan_Physical_Device device = devices[i];
        for (int j = 0; j < device.queue_family_properties.count; j++) {
            // NOTE: @Hack to make my vulkan program select my nvidia gpu
            // TODO: Remove this            
            VkQueueFamilyProperties properties = device.queue_family_properties[j];
            if ((properties.queueFlags & required_queue_type) &&
                ((bool)device.queue_supports_present[j] == supports_present) &&
                strstr(device.properties.deviceName, "NVIDIA")) {
                device_index = i;
                logprintf("Using graphics device %d and queue family %d\n\n", i, j);
                devices[i].graphics_and_present_queue_family_index = j;
                return;
            }
        }
    }

    logprintf("Required queue type %x and supports present %d not found!\n", required_queue_type, supports_present);
}

Vulkan_Physical_Device *Vulkan_Physical_Devices::get_selected() {
    if (device_index < 0) {
        logprintf("A physical device has not been selected!\n");
        return NULL;
    }

    return &devices[device_index];
}

static VkPresentModeKHR choose_present_mode(Array <VkPresentModeKHR> const &present_modes) {
    for (VkPresentModeKHR present_mode : present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR; // FIFO should always be supported.
}

static u32 choose_num_images(VkSurfaceCapabilitiesKHR capabilities) {
    u32 requested_num_images = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 && requested_num_images > capabilities.maxImageCount) {
        requested_num_images = capabilities.maxImageCount;
    }

    return requested_num_images;
}

static VkSurfaceFormatKHR choose_surface_format_and_color_space(Array <VkSurfaceFormatKHR> const &surface_formats) {
    for (VkSurfaceFormatKHR format : surface_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return surface_formats[0];
}

static VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format,
                                     VkImageAspectFlags aspect_flags,
                                     VkImageViewType view_type,
                                     u32 num_layers, u32 mip_levels) {
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image        = image;
    view_create_info.viewType     = view_type;
    view_create_info.format       = format;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.subresourceRange.aspectMask     = aspect_flags;
    view_create_info.subresourceRange.baseMipLevel   = 0;
    view_create_info.subresourceRange.levelCount     = mip_levels;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount     = num_layers;

    VkImageView result;

    if (vkCreateImageView(device, &view_create_info, NULL, &result) != VK_SUCCESS) {
        logprintf("Failed to create vulkan image view");
        return NULL;
    }
    
    return result;
}

bool Vulkan_Context::create_swap_chain() {
    VkSurfaceCapabilitiesKHR surface_capabilities = selected_physical_device->surface_capabilities;

    u32 num_images = choose_num_images(surface_capabilities);

    Array <VkPresentModeKHR> const &present_modes = selected_physical_device->present_modes;
    VkPresentModeKHR present_mode = choose_present_mode(present_modes);
    
    VkSurfaceFormatKHR surface_format = choose_surface_format_and_color_space(selected_physical_device->surface_formats);
    swap_chain_surface_format = surface_format;
    
    VkSwapchainCreateInfoKHR swap_chain_create_info = {};
    swap_chain_create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface          = surface;
    swap_chain_create_info.minImageCount    = num_images;
    swap_chain_create_info.imageFormat      = surface_format.format;
    swap_chain_create_info.imageColorSpace  = surface_format.colorSpace;
    swap_chain_create_info.imageExtent      = surface_capabilities.currentExtent;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage       = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_create_info.queueFamilyIndexCount = 1;
    swap_chain_create_info.pQueueFamilyIndices   = &queue_family;
    swap_chain_create_info.preTransform          = surface_capabilities.currentTransform;
    swap_chain_create_info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode           = present_mode;
    swap_chain_create_info.clipped               = VK_TRUE;

    CHECK_VK_RESULT(vkCreateSwapchainKHR(device, &swap_chain_create_info, NULL, &swap_chain), "Failed to create the swap chain");

    logprintf("Vulkan swap chain created!\n");

    u32 num_swap_chain_images = 0;
    CHECK_VK_RESULT(vkGetSwapchainImagesKHR(device, swap_chain, &num_swap_chain_images, NULL), "Failed to get number of swap chain images");
    Assert(num_images == num_swap_chain_images);

    logprintf("Number of swap chain images: %d\n", num_swap_chain_images);

    images.resize(num_swap_chain_images);
    image_views.resize(num_swap_chain_images);

    CHECK_VK_RESULT(vkGetSwapchainImagesKHR(device, swap_chain, &num_swap_chain_images, images.data), "Failed to get swap chain images");

    int num_layers = 1;
    int mip_levels = 1;
    for (u32 i = 0; i < num_swap_chain_images; i++) {
        image_views[i] = create_image_view(device, images[i], surface_format.format,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           VK_IMAGE_VIEW_TYPE_2D,
                                           num_layers, mip_levels);
    }

    return true;
}

bool Vulkan_Context::create_command_buffer_pool() {
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = queue_family;

    CHECK_VK_RESULT(vkCreateCommandPool(device, &create_info, NULL, &command_buffer_pool), "Failed to create command buffer pool");

    logprintf("Vulkan command buffer pool created!\n");

    return true;
}

bool Vulkan_Context::create_synchronization_objects() {
    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; i++) {
        CHECK_VK_RESULT(vkCreateFence(device, &fence_create_info, NULL, &fences[i]), "Failed to create vulkan fence!");
        CHECK_VK_RESULT(vkCreateSemaphore(device, &semaphore_create_info, NULL, &present_complete_semaphores[i]), "Failed to create vulkan present semaphore!");
    }

    render_complete_semaphores.resize(images.count);
    for (int i = 0; i < images.count; i++) {
        CHECK_VK_RESULT(vkCreateSemaphore(device, &semaphore_create_info, NULL, &render_complete_semaphores[i]), "Failed to create vulkan render semaphore!");
    }
    
    return true;
}

bool Vulkan_Context::create_depth_resources(Platform_Window *window) {
    int num_swap_chain_images = images.count;
    depth_images.resize(num_swap_chain_images);

    VkFormat depth_format = selected_physical_device->depth_format;

    for (int i = 0; i < num_swap_chain_images; i++) {
        VkImageUsageFlagBits usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VkMemoryPropertyFlagBits property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        depth_images[i] = create_image(window->width, window->height, depth_format, usage, property_flags);
        if (!depth_images[i].is_valid) return false;

        if (!transition_image_layout(depth_images[i].image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) return false;

        depth_images[i].view = create_image_view(device, depth_images[i].image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1, 1);
        if (!depth_images[i].view) return false;
    }

    logprintf("Depth images created!\n");

    return true;
}

Vulkan_Buffer Vulkan_Context::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo create_info = {};
    create_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size        = size;
    create_info.usage       = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo buffer_allocation_create_info = {};
    buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    
    Vulkan_Buffer result = {};

    if (vmaCreateBuffer(allocator, &create_info, &buffer_allocation_create_info, &result.buffer, &result.allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to create vulkan buffer!\n");
        return {};
    }
    logprintf("Vulkan buffer created!\n");

    result.is_valid = true;
    result.size     = (u32)size;

    return result;
}

bool Vulkan_Context::copy_buffer(VkBuffer destination, VkBuffer source, VkDeviceSize size) {
    if (!vulkan_begin_command_buffer(copy_command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) return false;

    VkBufferCopy buffer_copy = {};
    buffer_copy.srcOffset = 0;
    buffer_copy.dstOffset = 0;
    buffer_copy.size      = size;

    vkCmdCopyBuffer(copy_command_buffer, source, destination, 1, &buffer_copy);

    vkEndCommandBuffer(copy_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &copy_command_buffer;
    
    CHECK_VK_RESULT(vkQueueSubmit(graphics_and_present_queue, 1, &submit_info, NULL), "Failed to submit to graphics and present queue");
    
    vkQueueWaitIdle(graphics_and_present_queue);

    return true;
}

bool Vulkan_Context::create_command_buffers(int num_command_buffers, VkCommandBuffer *command_buffers) {
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_buffer_pool;
    allocate_info.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = num_command_buffers;

    CHECK_VK_RESULT(vkAllocateCommandBuffers(device, &allocate_info, command_buffers), "Failed to allocate command buffers");

    logprintf("%d vulkan command buffers allocated!\n", NUM_FRAMES_IN_FLIGHT);

    return true;
}

Vulkan_Buffer Vulkan_Context::create_vertex_index_buffer(VkDeviceSize vertex_buffer_size, void *vertex_buffer_data, VkDeviceSize index_buffer_size, void *index_buffer_data) {
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size  = vertex_buffer_size + index_buffer_size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VmaAllocationCreateInfo buffer_allocation_create_info = {};
    buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    Vulkan_Buffer result = {};
    
    if (vmaCreateBuffer(allocator, &buffer_create_info, &buffer_allocation_create_info, &result.buffer, &result.allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to create vulkan buffer!\n");
        return {};
    }

    result.size     = buffer_create_info.size;
    result.is_valid = true;

    void *mapped_buffer_data = NULL;
    if (vmaMapMemory(allocator, result.allocation, &mapped_buffer_data)) {
        logprintf("Failed to map vulkan buffer!\n");
        result.destroy(device);
        return {};
    }

    memcpy(mapped_buffer_data, vertex_buffer_data, vertex_buffer_size);
    memcpy(((u8 *)mapped_buffer_data) + vertex_buffer_size, index_buffer_data, index_buffer_size);

    vmaUnmapMemory(allocator, result.allocation);
    
    return result;
}

u32 Vulkan_Context::get_memory_type_index(u32 memory_type_bits_mask, VkMemoryPropertyFlags required_memory_property_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties = selected_physical_device->memory_properties;

    for (u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
        VkMemoryType memory_type = memory_properties.memoryTypes[i];
        u32 current_bitmask = Bit(i);
        bool is_current_memory_type_supported = memory_type_bits_mask & current_bitmask;
        bool has_required_memory_properties = (memory_type.propertyFlags & required_memory_property_flags) == required_memory_property_flags;

        if (is_current_memory_type_supported && has_required_memory_properties) {
            return i;
        }
    }

    logprintf("Failed to find memory type for type %x requested memory properties %x\n", memory_type_bits_mask, required_memory_property_flags);
    return UINT32_MAX;
}

bool Vulkan_Context::create_shader_device_address_buffers(Vulkan_Buffer buffers[NUM_FRAMES_IN_FLIGHT], VkDeviceSize size) {
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo buffer_create_info = {};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size  = size;
        buffer_create_info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        VmaAllocationCreateInfo buffer_allocation_create_info = {};
        buffer_allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        buffer_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

        CHECK_VK_RESULT(vmaCreateBuffer(allocator, &buffer_create_info, &buffer_allocation_create_info, &buffers[i].buffer, &buffers[i].allocation, NULL), "Failed to create vulkan shader device address buffer");
        CHECK_VK_RESULT(vmaMapMemory(allocator, buffers[i].allocation, &buffers[i].mapped_data), "Failed to map vulkan shader device address buffer");

        VkBufferDeviceAddressInfo buffer_device_address_info = {};
        buffer_device_address_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        buffer_device_address_info.buffer = buffers[i].buffer;

        buffers[i].device_address = vkGetBufferDeviceAddress(device, &buffer_device_address_info);
    }

    return true;
}

Vulkan_Texture Vulkan_Context::create_texture(String filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(0);
    stbi_uc *data = stbi_load(temp_c_string(filepath), &width, &height, &channels, 4);
    if (!data) {
        logprintf("Failed to load image '%s'\n", temp_c_string(filepath));
        return {};
    }
    defer { stbi_image_free(data); };

    Vulkan_Texture result = {};
    
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    // Create texture image from data(texture, data, width, height, format)
    result = create_texture_image_from_data(data, width, height, format);
    if (!result.is_valid) {
        return {};
    }
    
    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    result.view = create_image_view(device, result.image, format, aspect_flags, VK_IMAGE_VIEW_TYPE_2D, 1, 1);
    if (!result.view) {
        result.destroy(device);
        return {};
    }
    
    VkFilter min_filter = VK_FILTER_LINEAR;
    VkFilter mag_filter = VK_FILTER_LINEAR;
    VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    result.sampler = vulkan_create_texture_sampler(device, min_filter, mag_filter, address_mode);
    if (!result.sampler) {
        result.destroy(device);
        return {};
    }

    logprintf("Texture '%s' was created!\n", temp_c_string(filepath));
    
    result.is_valid = true;
    
    return result;
}

Vulkan_Texture Vulkan_Context::create_texture_image_from_data(void *data, int width, int height, VkFormat format) {
    VkImageUsageFlagBits usage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    VkMemoryPropertyFlagBits property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    Vulkan_Texture result = create_image(width, height, format, usage, property_flags);
    if (!result.is_valid) return {};

    update_texture_image(&result, width, height, format, data);

    return result;
}

Vulkan_Texture Vulkan_Context::create_image(int width, int height, VkFormat format, VkImageUsageFlags usage_flags, VkMemoryPropertyFlagBits property_flags) {
    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = format;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = usage_flags;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    Vulkan_Texture result;
    if (vmaCreateImage(allocator, &image_create_info, &allocation_create_info, &result.image, &result.allocation, NULL) != VK_SUCCESS) {
        logprintf("Failed to create vulkan image!\n");
        return {};
    }
    
    result.is_valid = true;
    
    return result;
}

bool Vulkan_Context::update_texture_image(Vulkan_Texture *texture, int width, int height, VkFormat format, void *data) {
    int bpp = get_bpp(format);

    VkDeviceSize layer_size = (VkDeviceSize)width * (VkDeviceSize)height * (VkDeviceSize)bpp;
    int layer_count = 1;
    VkDeviceSize image_size = layer_count * layer_size;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    Vulkan_Buffer staging_buffer = create_buffer(image_size, usage, properties);
    defer { staging_buffer.destroy(device); };
    staging_buffer.update(allocator, data, (u32)image_size);

    if (!transition_image_layout(texture->image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) return false;
    
    copy_buffer_to_image(texture->image, staging_buffer.buffer, width, height);
    transition_image_layout(texture->image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return true;
}

bool Vulkan_Context::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
    if (!vulkan_begin_command_buffer(copy_command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) return false;
    if (!vulkan_image_memory_barrier(copy_command_buffer, image, format, old_layout, new_layout)) return false;

    vkEndCommandBuffer(copy_command_buffer);
    
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &copy_command_buffer;
    
    CHECK_VK_RESULT(vkQueueSubmit(graphics_and_present_queue, 1, &submit_info, NULL), "Failed to submit to graphics and present queue");
    
    vkQueueWaitIdle(graphics_and_present_queue);
    
    return true;
}

// Copied from ogldev Vulkan For Beginners #18 19:09
bool vulkan_image_memory_barrier(VkCommandBuffer buffer, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
        format == VK_FORMAT_D16_UNORM ||
        format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
        format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_S8_UINT ||
        format == VK_FORMAT_D16_UNORM_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (has_stencil_component(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && 
		new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { /* Convert back from read-only to updateable */
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}  else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
		     new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { /* Convert from updateable texture to shader read-only */
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) { /* Convert depth texture from undefined state to depth-stencil buffer */
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { /* Wait for render pass to complete */
		barrier.srcAccessMask = 0; // VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = 0;
		/*
          source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
          ///		destination_stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
          destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		*/
		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { /* Convert back from read-only to color attachment */
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { /* Convert from updateable texture to shader read-only */
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) { /* Convert back from read-only to depth attachment */
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destination_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { /* Convert from updateable depth texture to shader read-only */
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		source_stage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	} else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = 0;

		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destination_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	} else {
		logprintf("Unknown barrier case\n");
        return false;
	}

    vkCmdPipelineBarrier(buffer, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

    return true;
}

VkSampler vulkan_create_texture_sampler(VkDevice device, VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode address_mode) {
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.minFilter = min_filter;
    sampler_info.magFilter = mag_filter;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.maxAnisotropy = 1;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    VkSampler result;
    if (vkCreateSampler(device, &sampler_info, VK_NULL_HANDLE, &result) != VK_SUCCESS) {
        logprintf("Failed to create vulkan texture sampler!\n");
        return NULL;
    }

    return result;
}

bool Vulkan_Context::copy_buffer_to_buffer(VkBuffer dst, VkBuffer src, VkDeviceSize size) {
    if (!vulkan_begin_command_buffer(copy_command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) return false;

    VkBufferCopy buffer_copy = {};
    buffer_copy.size = size;

    vkCmdCopyBuffer(copy_command_buffer, src, dst, 1, &buffer_copy);

    vkEndCommandBuffer(copy_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &copy_command_buffer;
    
    CHECK_VK_RESULT(vkQueueSubmit(graphics_and_present_queue, 1, &submit_info, NULL), "Failed to submit to graphics and present queue");
    
    vkQueueWaitIdle(graphics_and_present_queue);

    return true;
}

bool Vulkan_Context::copy_buffer_to_image(VkImage dst, VkBuffer src, int width, int height) {
    if (!vulkan_begin_command_buffer(copy_command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) return false;

    VkBufferImageCopy buffer_image_copy = {};
    buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_image_copy.imageSubresource.layerCount = 1;
    buffer_image_copy.imageExtent.width = width;
    buffer_image_copy.imageExtent.height = height;
    buffer_image_copy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(copy_command_buffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

    vkEndCommandBuffer(copy_command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &copy_command_buffer;
    
    CHECK_VK_RESULT(vkQueueSubmit(graphics_and_present_queue, 1, &submit_info, NULL), "Failed to submit to graphics and present queue");
    
    vkQueueWaitIdle(graphics_and_present_queue);
    
    return true;
}

bool vulkan_begin_command_buffer(VkCommandBuffer command_buffer, VkCommandBufferUsageFlags usage_flags) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = usage_flags;

    CHECK_VK_RESULT(vkBeginCommandBuffer(command_buffer, &begin_info), "Failed to begin command buffer");

    return true;
}

VkSemaphore vulkan_create_semaphore(VkDevice device) {
    VkSemaphoreCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore semaphore;
    if (vkCreateSemaphore(device, &create_info, NULL, &semaphore) != VK_SUCCESS) {
        logprintf("Failed to create vulkan semaphore\n");
        return NULL;
    }

    return semaphore;
}

VkShaderModule vulkan_create_shader_module_from_binary(VkDevice device, String filename) {
    const char *c_filename = temp_c_string(filename);
    
    FILE *file = fopen(c_filename, "rb");
    if (!file) {
        logprintf("Failed to read file '%s'\n", c_filename);
        return NULL;
    }
    defer { fclose(file); };

    fseek(file, 0, SEEK_END);
    auto file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8 *file_data = new u8[file_size];
    defer { delete [] file_data; };

    fread(file_data, 1, file_size, file);

    VkShaderModuleCreateInfo shader_create_info = {};
    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.codeSize = file_size;
    shader_create_info.pCode = (const u32 *)file_data;

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &shader_create_info, NULL, &shader_module) != VK_SUCCESS) {
        logprintf("Failed to create a shader module for '%s'!\n", c_filename);
        return NULL;
    }

    return shader_module;
}

bool Vulkan_Graphics_Pipeline::init(VkDevice device, VkShaderModule vs, VkShaderModule fs, VkFormat color_format, VkFormat depth_format, int num_submeshes) {
    VkPipelineShaderStageCreateInfo shader_stage_create_infos[2] = {};

    shader_stage_create_infos[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_create_infos[0].module = vs;
    shader_stage_create_infos[0].pName  = "main";

    shader_stage_create_infos[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_infos[1].module = fs;
    shader_stage_create_infos[1].pName  = "main";

    VkVertexInputBindingDescription vertex_binding = {};
    vertex_binding.binding   = 0;
    vertex_binding.stride    = sizeof(Mesh_Vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertex_attributes[5] = {};

    vertex_attributes[0].location = 0;
    vertex_attributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset   = offsetof(Mesh_Vertex, position);

    vertex_attributes[1].location = 1;
    vertex_attributes[1].format   = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[1].offset   = offsetof(Mesh_Vertex, uv);

    vertex_attributes[2].location = 2;
    vertex_attributes[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[2].offset   = offsetof(Mesh_Vertex, normal);

    vertex_attributes[3].location = 3;
    vertex_attributes[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[3].offset   = offsetof(Mesh_Vertex, tangent);

    vertex_attributes[4].location = 4;
    vertex_attributes[4].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[4].offset   = offsetof(Mesh_Vertex, bitangent);
    
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &vertex_binding;
    vertex_input_info.vertexAttributeDescriptionCount = ArrayCount(vertex_attributes);
    vertex_input_info.pVertexAttributeDescriptions = vertex_attributes;
    
    VkPipelineInputAssemblyStateCreateInfo pipeline_ia_create_info = {};
    pipeline_ia_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_ia_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_ia_create_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo vp_create_info = {};
    vp_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_create_info.viewportCount = 1;
    vp_create_info.scissorCount = 1;

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = ArrayCount(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;
    
    VkPipelineRasterizationStateCreateInfo rasterization_create_info = {};
    rasterization_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_create_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo pipeline_ms_create_info = {};
    pipeline_ms_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipeline_ms_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipeline_ms_create_info.sampleShadingEnable = VK_FALSE;
    pipeline_ms_create_info.minSampleShading = 1.0f;

    VkPipelineColorBlendAttachmentState blend_attachment_state = {};
    blend_attachment_state.blendEnable = VK_FALSE;
    blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
    depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.depthTestEnable = VK_TRUE;
    depth_stencil_state.depthWriteEnable = VK_TRUE;
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil_state.maxDepthBounds = 1.0f;
    
    VkPipelineColorBlendStateCreateInfo blend_create_info = {};
    blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_create_info.logicOpEnable = VK_FALSE;
    blend_create_info.logicOp = VK_LOGIC_OP_COPY;
    blend_create_info.attachmentCount = 1;
    blend_create_info.pAttachments = &blend_attachment_state;

    VkPipelineRenderingCreateInfo rendering_info = {};
    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &color_format;
    rendering_info.depthAttachmentFormat = depth_format;
    rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkDescriptorBindingFlags descriptor_variable_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo descriptor_binding_flags = {};
    descriptor_binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    descriptor_binding_flags.bindingCount = 1;
    descriptor_binding_flags.pBindingFlags = &descriptor_variable_flags;

    VkDescriptorSetLayoutBinding descriptor_layout_binding_texture = {};
    descriptor_layout_binding_texture.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_layout_binding_texture.descriptorCount = 1;
    descriptor_layout_binding_texture.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info = {};
    descriptor_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout_create_info.pNext = &descriptor_binding_flags;
    descriptor_layout_create_info.bindingCount = 1;
    descriptor_layout_create_info.pBindings = &descriptor_layout_binding_texture;

    CHECK_VK_RESULT(vkCreateDescriptorSetLayout(device, &descriptor_layout_create_info, NULL, &descriptor_set_layout), "Failed to create texture descriptor set layout");

    VkDescriptorPoolSize pool_size = {};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = num_submeshes;

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.maxSets = num_submeshes;
    descriptor_pool_create_info.poolSizeCount = 1;
    descriptor_pool_create_info.pPoolSizes = &pool_size;

    CHECK_VK_RESULT(vkCreateDescriptorPool(device, &descriptor_pool_create_info, NULL, &descriptor_pool), "Failed to create vulkan descriptor pool");

    u32 *variable_descriptor_counts = TAllocArray(u32, num_submeshes);
    VkDescriptorSetLayout *descriptor_set_layouts = TAllocArray(VkDescriptorSetLayout, num_submeshes);
    for (int i = 0; i < num_submeshes; i++) {
        descriptor_set_layouts[i] = descriptor_set_layout;
        variable_descriptor_counts[i] = 1;
    }
    
    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_allocate_info = {};
    variable_descriptor_count_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variable_descriptor_count_allocate_info.descriptorSetCount = num_submeshes;
    variable_descriptor_count_allocate_info.pDescriptorCounts = variable_descriptor_counts;

    
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.pNext = &variable_descriptor_count_allocate_info;
    descriptor_set_allocate_info.descriptorPool = descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = num_submeshes;
    descriptor_set_allocate_info.pSetLayouts = descriptor_set_layouts;

    descriptor_sets.resize(num_submeshes);
    CHECK_VK_RESULT(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, descriptor_sets.data), "Failed to allocate vulkan descriptor sets!");

    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset     = 0;
    push_constant_range.size       = 2 * sizeof(VkDeviceAddress);
    
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &descriptor_set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant_range;
    
    CHECK_VK_RESULT(vkCreatePipelineLayout(device, &layout_info, NULL, &pipeline_layout), "Failed to create graphics pipeline layout");
    
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = &rendering_info;
    pipeline_info.stageCount = ArrayCount(shader_stage_create_infos);
    pipeline_info.pStages = shader_stage_create_infos;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &pipeline_ia_create_info;
    pipeline_info.pViewportState = &vp_create_info;
    pipeline_info.pRasterizationState = &rasterization_create_info;
    pipeline_info.pMultisampleState = &pipeline_ms_create_info;
    pipeline_info.pDepthStencilState = &depth_stencil_state;
    pipeline_info.pColorBlendState = &blend_create_info;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;
    pipeline_info.pDynamicState = &dynamic_state;
    
    CHECK_VK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline), "Failed to create graphics pipeline");

    logprintf("Vulkan graphics pipeline created!\n");
    
    return true;
}

void Vulkan_Graphics_Pipeline::update_descriptor_set(VkDevice device, VkDescriptorSet set, Vulkan_Texture texture) {
    VkDescriptorImageInfo image_info = {};
    image_info.sampler     = texture.sampler;
    image_info.imageView   = texture.view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet wds = {};
    wds.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstSet          = set;
    wds.dstBinding      = 0;
    wds.dstArrayElement = 0;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo      = &image_info;

    vkUpdateDescriptorSets(device, 1, &wds, 0, NULL);
}

bool Vulkan_Buffer::update(VmaAllocator allocator, void *data, VkDeviceSize size) {
    void *mapped_memory;
    CHECK_VK_RESULT(vmaMapMemory(allocator, allocation, &mapped_memory), "Failed to map buffer memory");
    memcpy(mapped_memory, data, size);
    vmaUnmapMemory(allocator, allocation);

    return true;
}

void Vulkan_Buffer::destroy(VkDevice device) {
	if (buffer) {
		vkDestroyBuffer(device, buffer, NULL);
	}

    is_valid = false;
}

void Vulkan_Texture::destroy(VkDevice device) {
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, sampler, NULL);
        sampler = VK_NULL_HANDLE;
    }

    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, NULL);
        view = VK_NULL_HANDLE;
    }

    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, NULL);
        image = VK_NULL_HANDLE;
    }
}

void vulkan_cmd_bind_pipeline(VkCommandBuffer command_buffer, Vulkan_Graphics_Pipeline *pipeline, int image_index, Array <VkDescriptorSet> const &per_scene_descriptor_sets, Array <VkDescriptorSet> const &descriptor_sets) {
    Assert(pipeline);
    
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    if (descriptor_sets.count > 0) {
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->pipeline_layout, 0, 1,
                                &per_scene_descriptor_sets[image_index], 0, NULL);
        
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->pipeline_layout, 1, 1,
                                &descriptor_sets[image_index], 0, NULL);
    }
}
