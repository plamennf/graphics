#include "pch.h"

#include "vulkan_context.h"

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#endif

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
    app_info.apiVersion         = VK_API_VERSION_1_0;

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

        u32 api_version = device->properties.apiVersion;

        logprintf("    API Version: %d.%d.%d.%d\n",
                  VK_API_VERSION_VARIANT(api_version),
                  VK_API_VERSION_MAJOR(api_version),
                  VK_API_VERSION_MINOR(api_version),
                  VK_API_VERSION_PATCH(api_version));

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
    }

    return true;
}

u32 Vulkan_Physical_Devices::select_device(VkQueueFlags required_queue_type, bool supports_present) {
    for (int i = 0; i < devices.count; i++) {
        Vulkan_Physical_Device device = devices[i];
        for (int j = 0; j < device.queue_family_properties.count; j++) {
            VkQueueFamilyProperties properties = device.queue_family_properties[j];
            if ((properties.queueFlags & required_queue_type) &&
                ((bool)device.queue_supports_present[j] == supports_present)) {
                device_index = i;
                logprintf("Using graphics device %d and queue family %d\n", i, j);
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
