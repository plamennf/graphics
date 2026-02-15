#include "pch.h"

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
    queue_family = physical_devices.select_device(VK_QUEUE_GRAPHICS_BIT, true);
    selected_physical_device = physical_devices.get_selected();
    if (!selected_physical_device) return false;
    if (!create_device()) return false;
    if (!create_swap_chain()) return false;
    if (!create_command_buffer_pool()) return false;
    if (!command_queue.init(device, swap_chain, queue_family, 0)) return false;
    if (!create_command_buffers(1, &copy_command_buffer)) return false;
    if (!create_depth_resources(window)) return false;
    
    return true;
}

bool Vulkan_Context::get_instance_version() {
    u32 instance_version_packed = 0;

    CHECK_VK_RESULT(vkEnumerateInstanceVersion(&instance_version_packed), "Failed to enumerate the vulkan instance version");

    instance_version.major = VK_API_VERSION_MAJOR(instance_version_packed);
    instance_version.minor = VK_API_VERSION_MINOR(instance_version_packed);
    instance_version.patch = VK_API_VERSION_PATCH(instance_version_packed);

    logprintf("Vulkan loader supports version %d.%d.%d\n", instance_version.major, instance_version.minor, instance_version.patch);

    return true;
}

bool Vulkan_Context::create_instance() {
    if (!get_instance_version()) return false;
    
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
    app_info.apiVersion         = VK_MAKE_API_VERSION(0, instance_version.major, instance_version.minor, 0);

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

    bool device_supports_dynamic_rendering = selected_physical_device->is_extension_supported(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    bool instance_is_1_3_or_more = instance_version.major > 1 || instance_version.minor >= 3;

    if (device_supports_dynamic_rendering && instance_is_1_3_or_more) {
        logprintf("The vulkan instance and device support dynamic rendering as a core feature!\n");
    } else if (instance_version.minor == 2) {
        if (device_supports_dynamic_rendering) {
            device_extensions.add(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        } else {
            logprintf("The system doesn't support dynamic rendering!\n");
            return false;
        }
    } else {
        logprintf("The system doesn't support dynamic rendering!\n");
        return false;
    }
    
    if (selected_physical_device->features.geometryShader == VK_FALSE) {
        logprintf("The geometry shader is not supported!\n");
        return false;
    }

    if (selected_physical_device->features.tessellationShader == VK_FALSE) {
        logprintf("The tessellation shader is not supported!\n");
        return false;
    }

    VkPhysicalDeviceFeatures device_features = {};
    device_features.geometryShader     = VK_TRUE;
    device_features.tessellationShader = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
    dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_rendering_features.dynamicRendering = VK_TRUE;
    
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext                   = &dynamic_rendering_features;
    device_create_info.queueCreateInfoCount    = 1;
    device_create_info.pQueueCreateInfos       = &queue_create_info;
    device_create_info.enabledExtensionCount   = device_extensions.count;
    device_create_info.ppEnabledExtensionNames = device_extensions.data;
    device_create_info.pEnabledFeatures        = &device_features;

    CHECK_VK_RESULT(vkCreateDevice(selected_physical_device->device, &device_create_info, NULL, &device), "Failed to create a vulkan logical device");

    logprintf("\nVulkan logical device created!\n");

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

        get_device_api_version(i);
        get_extensions(i);
        
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

u32 Vulkan_Physical_Devices::select_device(VkQueueFlags required_queue_type, bool supports_present) {
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
                return j;
            }
        }
    }

    logprintf("Required queue type %x and supports present %d not found!\n", required_queue_type, supports_present);

    return 0;
}

Vulkan_Physical_Device *Vulkan_Physical_Devices::get_selected() {
    if (device_index < 0) {
        logprintf("A physical device has not been selected!\n");
        return NULL;
    }

    return &devices[device_index];
}

void Vulkan_Physical_Devices::get_extensions(int index) {
    Vulkan_Physical_Device *device = &devices[index];
    Assert(device);
    
    u32 num_extensions;
    vkEnumerateDeviceExtensionProperties(device->device, NULL, &num_extensions, NULL);
    Assert(num_extensions > 0);

    device->extensions.resize(num_extensions);

    vkEnumerateDeviceExtensionProperties(device->device, NULL, &num_extensions, device->extensions.data);

    logprintf("    Physical device extensions:\n");
    for (VkExtensionProperties const &extension : device->extensions) {
        logprintf("      %s\n", extension.extensionName);
    }
}

void Vulkan_Physical_Devices::get_device_api_version(int index) {
    Vulkan_Physical_Device *device = &devices[index];
    Assert(device);
    
    u32 api_version = device->properties.apiVersion;

    device->api_version.variant = VK_API_VERSION_VARIANT(api_version);
    device->api_version.major   = VK_API_VERSION_MAJOR(api_version);
    device->api_version.minor   = VK_API_VERSION_MINOR(api_version);
    device->api_version.patch   = VK_API_VERSION_PATCH(api_version);
    
    logprintf("    API Version: %d.%d.%d.%d\n",
              VK_API_VERSION_VARIANT(api_version),
              VK_API_VERSION_MAJOR(api_version),
              VK_API_VERSION_MINOR(api_version),
              VK_API_VERSION_PATCH(api_version));
}

bool Vulkan_Physical_Device::is_extension_supported(const char *extension) const {
    for (VkExtensionProperties const &e : extensions) {
        if (strings_match(e.extensionName, extension)) {
            return true;
        }
    }

    return false;
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

bool Vulkan_Context::create_command_buffers(int num_command_buffers, VkCommandBuffer *command_buffers) {
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_buffer_pool;
    allocate_info.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = num_command_buffers;

    CHECK_VK_RESULT(vkAllocateCommandBuffers(device, &allocate_info, command_buffers), "Failed to allocate command buffers");

    logprintf("%d vulkan command buffers allocated!\n", num_command_buffers);

    return true;
}

Vulkan_Buffer_And_Memory Vulkan_Context::create_vertex_buffer(void *data, u32 size) {
    // Create the staging buffer and copy the memory to it

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    Vulkan_Buffer_And_Memory staging_buffer = create_buffer(size, usage, memory_properties);
    defer { staging_buffer.destroy(device); };
    
    void *staging_buffer_memory = NULL;
    if (vkMapMemory(device, staging_buffer.memory, 0, staging_buffer.allocation_size, 0, &staging_buffer_memory) != VK_SUCCESS) {
        logprintf("Failed to create a staging buffer for the vertex buffer!\n");
        return {};
    }
    memcpy(staging_buffer_memory, data, size);
    vkUnmapMemory(device, staging_buffer.memory);

    usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    Vulkan_Buffer_And_Memory result = create_buffer(size, usage, memory_properties);

    if (!copy_buffer(result.buffer, staging_buffer.buffer, size)) {
        result.destroy(device);
        return {};
    }

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

Vulkan_Buffer_And_Memory Vulkan_Context::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo create_info = {};
    create_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size        = size;
    create_info.usage       = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Vulkan_Buffer_And_Memory result = {};

    if (vkCreateBuffer(device, &create_info, NULL, &result.buffer) != VK_SUCCESS) {
        logprintf("Failed to create a vulkan buffer!\n");
        return {};
    }
    logprintf("Vulkan buffer created!\n");

    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(device, result.buffer, &memory_requirements);
    logprintf("Vulkan buffer requires %d bytes\n", (int)memory_requirements.size);

    result.allocation_size = memory_requirements.size;

    u32 memory_type_index = get_memory_type_index(memory_requirements.memoryTypeBits, properties);
    if (memory_type_index == UINT32_MAX) {
        return {};
    }
    
    logprintf("Memory type index %d\n", memory_type_index);

    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_type_index;

    if (vkAllocateMemory(device, &memory_allocate_info, NULL, &result.memory) != VK_SUCCESS) {
        logprintf("Failed to allocate memory for vulkan buffer!\n");
        result.destroy(device);
        return {};
    }

    if (vkBindBufferMemory(device, result.buffer, result.memory, 0) != VK_SUCCESS) {
        logprintf("Failed to bind vulkan buffer memory!\n");
        result.destroy(device);
        return {};
    }

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

    command_queue.submit_sync(copy_command_buffer);
    command_queue.wait_idle();

    return true;
}

bool Vulkan_Context::create_uniform_buffers(Array <Vulkan_Buffer_And_Memory> &buffers, u32 size) {
    buffers.resize(images.count);

    for (int i = 0; i < buffers.count; i++) {
        buffers[i] = create_uniform_buffer(size);
        if (!buffers[i].is_valid) return false;
    }

    return true;
}

Vulkan_Buffer_And_Memory Vulkan_Context::create_uniform_buffer(VkDeviceSize size) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer(size, usage, memory_properties);
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

    Vulkan_Texture result;
    if (vkCreateImage(device, &image_create_info, NULL, &result.image) != VK_SUCCESS) {
        logprintf("Failed to create vulkan image!\n");
        result.destroy(device);
        return {};
    }

    VkMemoryRequirements memory_requirements = {};
    vkGetImageMemoryRequirements(device, result.image, &memory_requirements);
    logprintf("Image requires %d bytes\n", (int)memory_requirements.size);

    u32 memory_type_index = get_memory_type_index(memory_requirements.memoryTypeBits, property_flags);
    logprintf("Image memory type index: %d\n", memory_type_index);

    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = memory_type_index;

    if (vkAllocateMemory(device, &memory_allocate_info, NULL, &result.memory) != VK_SUCCESS) {
        logprintf("Failed to allocate memory for vulkan texture!\n");
        result.destroy(device);
        return {};
    }

    if (vkBindImageMemory(device, result.image, result.memory, 0) != VK_SUCCESS) {
        logprintf("Failed to bind memory to vulkan texture!\n");
        result.destroy(device);
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
    Vulkan_Buffer_And_Memory staging_buffer = create_buffer(image_size, usage, properties);
    defer { staging_buffer.destroy(device); };
    staging_buffer.update(device, data, (u32)image_size);

    if (!transition_image_layout(texture->image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) return false;
    
    copy_buffer_to_image(texture->image, staging_buffer.buffer, width, height);
    transition_image_layout(texture->image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return true;
}

bool Vulkan_Context::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout) {
    if (!vulkan_begin_command_buffer(copy_command_buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)) return false;
    if (!vulkan_image_memory_barrier(copy_command_buffer, image, format, old_layout, new_layout)) return false;
    submit_copy_command();
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

    submit_copy_command();

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

    submit_copy_command();

    return true;
}

void Vulkan_Context::submit_copy_command() {
    vkEndCommandBuffer(copy_command_buffer);
    command_queue.submit_sync(copy_command_buffer);
    command_queue.wait_idle();
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

bool Vulkan_Queue::init(VkDevice device, VkSwapchainKHR swap_chain, u32 queue_family, u32 queue_index) {
    this->device     = device;
    this->swap_chain = swap_chain;

    vkGetDeviceQueue(device, queue_family, queue_index, &queue);

    logprintf("Vulkan command queue acquired!\n");

    present_complete_semaphore = vulkan_create_semaphore(device);
    if (!present_complete_semaphore) return false;
    
    render_complete_semaphore  = vulkan_create_semaphore(device);
    if (!render_complete_semaphore) return false;
    
    return true;
}

int Vulkan_Queue::acquire_next_image() {
    u32 image_index = 0;
    if (vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, present_complete_semaphore, NULL, &image_index) != VK_SUCCESS) {
        logprintf("Failed to acquire a vulkan image\n");
        return -1;
    }

    return (int)image_index;
}

bool Vulkan_Queue::submit_sync(VkCommandBuffer command_buffer) {
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    CHECK_VK_RESULT(vkQueueSubmit(queue, 1, &submit_info, NULL), "Failed to submit command buffer to queue synchronized");

    return true;
}

bool Vulkan_Queue::submit_async(VkCommandBuffer command_buffer) {
    VkPipelineStageFlags wait_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &present_complete_semaphore;
    submit_info.pWaitDstStageMask = &wait_flags;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    CHECK_VK_RESULT(vkQueueSubmit(queue, 1, &submit_info, NULL), "Failed to submit command buffer to queue asynchronous");

    return true;
}

bool Vulkan_Queue::present(int _image_index) {
    u32 image_index = (u32)_image_index;
    
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swap_chain;
    present_info.pImageIndices = &image_index;

    CHECK_VK_RESULT(vkQueuePresentKHR(queue, &present_info), "Failed to queue present command");

    wait_idle();
    
    return true;
}

void Vulkan_Queue::wait_idle() {
    vkQueueWaitIdle(queue);
}

bool Vulkan_Graphics_Pipeline::init(VkShaderModule vs, VkShaderModule fs, VkFormat color_format, VkFormat depth_format) {
    this->device = device;
    
    VkPipelineShaderStageCreateInfo shader_stage_create_infos[2] = {};

    shader_stage_create_infos[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_create_infos[0].module = vs;
    shader_stage_create_infos[0].pName  = "main";

    shader_stage_create_infos[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_infos[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stage_create_infos[1].module = fs;
    shader_stage_create_infos[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo pipeline_ia_create_info = {};
    pipeline_ia_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_ia_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_ia_create_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.width    = (float)window->width;
    viewport.height   = (float)window->height;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent.width  = window->width;
    scissor.extent.height = window->height;

    VkPipelineViewportStateCreateInfo vp_create_info = {};
    vp_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_create_info.viewportCount = 1;
    vp_create_info.pViewports = &viewport;
    vp_create_info.scissorCount = 1;
    vp_create_info.pScissors = &scissor;

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
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
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
    
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkDescriptorSetLayout layouts[] = {
        per_scene_descriptor_set_layout,
        descriptor_set_layout,
    };
    
    layout_info.setLayoutCount = ArrayCount(layouts);
    layout_info.pSetLayouts = layouts;
    
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
    
    CHECK_VK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline), "Failed to create graphics pipeline");

    logprintf("Vulkan graphics pipeline created!\n");
    
    return true;
}

bool Vulkan_Graphics_Pipeline::allocate_descriptor_sets(Array <VkDescriptorSet> &descriptor_sets, int num_images) {
    // Create descriptor pool
    VkDescriptorPoolSize pool_sizes[NUM_PER_OBJECT_BINDINGS] = {};
    
    pool_sizes[PER_OBJECT_BINDING_VERTEX_BUFFER].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[PER_OBJECT_BINDING_VERTEX_BUFFER].descriptorCount = num_images;

    pool_sizes[PER_OBJECT_BINDING_INDEX_BUFFER].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[PER_OBJECT_BINDING_INDEX_BUFFER].descriptorCount = num_images;

    pool_sizes[PER_OBJECT_BINDING_UNIFORM].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[PER_OBJECT_BINDING_UNIFORM].descriptorCount = num_images;

    pool_sizes[PER_OBJECT_BINDING_TEXTURE].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[PER_OBJECT_BINDING_TEXTURE].descriptorCount = num_images;
    
    VkDescriptorPoolCreateInfo pool_create_info = {};
    pool_create_info.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.maxSets = (u32)num_images;
    pool_create_info.poolSizeCount = ArrayCount(pool_sizes);
    pool_create_info.pPoolSizes = pool_sizes;
    CHECK_VK_RESULT(vkCreateDescriptorPool(device, &pool_create_info, NULL, &descriptor_pool), "Failed to create vulkan descriptor pool");
    logprintf("Vulkan descriptor pool created!\n");
    
    // Create descriptor set layout
    Array <VkDescriptorSetLayoutBinding> layout_bindings;
    layout_bindings.use_temporary_storage = true;
    
    VkDescriptorSetLayoutBinding vertex_shader_layout_binding_vertex_buffer = {};
    vertex_shader_layout_binding_vertex_buffer.binding = PER_OBJECT_BINDING_VERTEX_BUFFER;
    vertex_shader_layout_binding_vertex_buffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertex_shader_layout_binding_vertex_buffer.descriptorCount = 1;
    vertex_shader_layout_binding_vertex_buffer.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_bindings.add(vertex_shader_layout_binding_vertex_buffer);

    VkDescriptorSetLayoutBinding vertex_shader_layout_binding_index_buffer = {};
    vertex_shader_layout_binding_index_buffer.binding = PER_OBJECT_BINDING_INDEX_BUFFER;
    vertex_shader_layout_binding_index_buffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertex_shader_layout_binding_index_buffer.descriptorCount = 1;
    vertex_shader_layout_binding_index_buffer.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_bindings.add(vertex_shader_layout_binding_index_buffer);

    VkDescriptorSetLayoutBinding vertex_shader_layout_binding_uniform_buffer = {};
    vertex_shader_layout_binding_uniform_buffer.binding = PER_OBJECT_BINDING_UNIFORM;
    vertex_shader_layout_binding_uniform_buffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vertex_shader_layout_binding_uniform_buffer.descriptorCount = 1;
    vertex_shader_layout_binding_uniform_buffer.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_bindings.add(vertex_shader_layout_binding_uniform_buffer);

    VkDescriptorSetLayoutBinding fragment_shader_layout_binding = {};
    fragment_shader_layout_binding.binding = PER_OBJECT_BINDING_TEXTURE;
    fragment_shader_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    fragment_shader_layout_binding.descriptorCount = 1;
    fragment_shader_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings.add(fragment_shader_layout_binding);    
    
    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = (u32)layout_bindings.count;
    layout_info.pBindings = layout_bindings.data;

    CHECK_VK_RESULT(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &descriptor_set_layout), "Failed to create vulkan descriptor set layout");
    
    // Allocate descriptor sets(num images)
    Array <VkDescriptorSetLayout> layouts;
    layouts.use_temporary_storage = true;
    layouts.resize(num_images);
    for (int i = 0; i < layouts.count; i++) {
        layouts[i] = descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = descriptor_pool;
    allocate_info.descriptorSetCount = (u32)num_images;
    allocate_info.pSetLayouts = layouts.data;

    descriptor_sets.resize(num_images);
    
    CHECK_VK_RESULT(vkAllocateDescriptorSets(device, &allocate_info, descriptor_sets.data), "Failed to allocate vulkan descriptor sets");
        
    return true;
}

void Vulkan_Graphics_Pipeline::update_descriptor_sets(Vulkan_Buffer_And_Memory vertex_buffer, Vulkan_Buffer_And_Memory index_buffer, Array <Vulkan_Buffer_And_Memory> const &uniform_buffers, Vulkan_Texture texture, Array <VkDescriptorSet> &descriptor_sets) {
    Array <VkWriteDescriptorSet> write_descriptor_set;
    write_descriptor_set.use_temporary_storage = true;
    write_descriptor_set.resize(descriptor_sets.count * NUM_PER_OBJECT_BINDINGS);

    VkDescriptorBufferInfo buffer_info_vertex_buffer = {};
    buffer_info_vertex_buffer.buffer = vertex_buffer.buffer;
    buffer_info_vertex_buffer.range  = vertex_buffer.size;

    VkDescriptorBufferInfo buffer_info_index_buffer = {};
    buffer_info_index_buffer.buffer = index_buffer.buffer;
    buffer_info_index_buffer.range  = index_buffer.size;

    Array <VkDescriptorBufferInfo> buffer_info_uniforms;
    buffer_info_uniforms.use_temporary_storage = true;
    buffer_info_uniforms.resize(descriptor_sets.count);

    for (int i = 0; i < descriptor_sets.count; i++) {
        VkDescriptorBufferInfo info = {};
        info.buffer = uniform_buffers[i].buffer;
        info.range  = uniform_buffers[i].size;
        buffer_info_uniforms[i] = info;
    }

    VkDescriptorImageInfo image_info = {};
    image_info.sampler     = texture.sampler;
    image_info.imageView   = texture.view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    int wds_index = 0;
    
    for (int i = 0; i < descriptor_sets.count; i++) {
        VkDescriptorSet dst_set = descriptor_sets[i];

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.dstSet = dst_set;
        wds.dstBinding = PER_OBJECT_BINDING_VERTEX_BUFFER;
        wds.dstArrayElement = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds.pBufferInfo = &buffer_info_vertex_buffer;

        Assert(wds_index < write_descriptor_set.count);
        write_descriptor_set[wds_index++] = wds;

        wds.dstBinding = PER_OBJECT_BINDING_INDEX_BUFFER;
        wds.pBufferInfo = &buffer_info_index_buffer;

        Assert(wds_index < write_descriptor_set.count);
        write_descriptor_set[wds_index++] = wds;

        wds.dstBinding = PER_OBJECT_BINDING_UNIFORM;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &buffer_info_uniforms[i];
            
        Assert(wds_index < write_descriptor_set.count);
        write_descriptor_set[wds_index++] = wds;

        wds.dstBinding = PER_OBJECT_BINDING_TEXTURE;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wds.pImageInfo = &image_info;

        Assert(wds_index < write_descriptor_set.count);
        write_descriptor_set[wds_index++] = wds;
    }

    vkUpdateDescriptorSets(device, (u32)write_descriptor_set.count, write_descriptor_set.data, 0, NULL);
}

// Copy-paste from above
bool Vulkan_Graphics_Pipeline::allocate_per_scene_descriptor_sets(Array <VkDescriptorSet> &descriptor_sets, int num_images) {
    VkDescriptorPoolSize pool_sizes[NUM_PER_SCENE_BINDINGS] = {};

    pool_sizes[PER_SCENE_BINDING_UNIFORM].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[PER_SCENE_BINDING_UNIFORM].descriptorCount = num_images;

    VkDescriptorPoolCreateInfo pool_create_info = {};
    pool_create_info.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.maxSets = (u32)num_images;
    pool_create_info.poolSizeCount = ArrayCount(pool_sizes);
    pool_create_info.pPoolSizes = pool_sizes;
    CHECK_VK_RESULT(vkCreateDescriptorPool(device, &pool_create_info, NULL, &per_scene_descriptor_pool), "Failed to create vulkan per scene descriptor pool");
    logprintf("Vulkan per scene descriptor pool created!\n");

    Array <VkDescriptorSetLayoutBinding> layout_bindings;
    layout_bindings.use_temporary_storage = true;

    VkDescriptorSetLayoutBinding vertex_shader_layout_binding_uniform_buffer = {};
    vertex_shader_layout_binding_uniform_buffer.binding = PER_SCENE_BINDING_UNIFORM;
    vertex_shader_layout_binding_uniform_buffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vertex_shader_layout_binding_uniform_buffer.descriptorCount = 1;
    vertex_shader_layout_binding_uniform_buffer.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_bindings.add(vertex_shader_layout_binding_uniform_buffer);

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = (u32)layout_bindings.count;
    layout_info.pBindings = layout_bindings.data;
    
    CHECK_VK_RESULT(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &per_scene_descriptor_set_layout), "Failed to create vulkan per scene descriptor set layout");

    Array <VkDescriptorSetLayout> layouts;
    layouts.use_temporary_storage = true;
    layouts.resize(num_images);
    for (int i = 0; i < layouts.count; i++) {
        layouts[i] = per_scene_descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = per_scene_descriptor_pool;
    allocate_info.descriptorSetCount = (u32)num_images;
    allocate_info.pSetLayouts = layouts.data;

    descriptor_sets.resize(num_images);
    
    CHECK_VK_RESULT(vkAllocateDescriptorSets(device, &allocate_info, descriptor_sets.data), "Failed to allocate vulkan descriptor sets");
        
    return true;

}

void Vulkan_Graphics_Pipeline::update_per_scene_descriptor_sets(Array <Vulkan_Buffer_And_Memory> const &uniform_buffers, Array <VkDescriptorSet> &descriptor_sets) {
    Array <VkWriteDescriptorSet> write_descriptor_set;
    write_descriptor_set.use_temporary_storage = true;
    write_descriptor_set.resize(descriptor_sets.count * NUM_PER_SCENE_BINDINGS);

    Array <VkDescriptorBufferInfo> buffer_info_uniforms;
    buffer_info_uniforms.use_temporary_storage = true;
    buffer_info_uniforms.resize(descriptor_sets.count);

    for (int i = 0; i < descriptor_sets.count; i++) {
        VkDescriptorBufferInfo info = {};
        info.buffer = uniform_buffers[i].buffer;
        info.range  = uniform_buffers[i].size;
        buffer_info_uniforms[i] = info;
    }

    int wds_index = 0;
    
    for (int i = 0; i < descriptor_sets.count; i++) {
        VkDescriptorSet dst_set = descriptor_sets[i];

        VkWriteDescriptorSet wds = {};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.dstSet = dst_set;
        wds.dstBinding = PER_SCENE_BINDING_UNIFORM;
        wds.dstArrayElement = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wds.pBufferInfo = &buffer_info_uniforms[i];
            
        Assert(wds_index < write_descriptor_set.count);
        write_descriptor_set[wds_index++] = wds;
    }

    vkUpdateDescriptorSets(device, (u32)write_descriptor_set.count, write_descriptor_set.data, 0, NULL);
}

bool Vulkan_Buffer_And_Memory::update(VkDevice device, void *data, u32 size) {
    void *mapped_memory;
    CHECK_VK_RESULT(vkMapMemory(device, memory, 0, size, 0, &mapped_memory), "Failed to map buffer memory");
    memcpy(mapped_memory, data, size);
    vkUnmapMemory(device, memory);

    return true;
}

void Vulkan_Buffer_And_Memory::destroy(VkDevice device) {
    if (memory) {
		vkFreeMemory(device, memory, NULL);
	}
    
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

    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, NULL);
        memory = VK_NULL_HANDLE;
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
