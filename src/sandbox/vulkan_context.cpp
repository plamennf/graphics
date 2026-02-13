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
    selected_physical_device = physical_devices.get_selected();
    if (!selected_physical_device) return false;
    if (!create_device()) return false;
    if (!create_swap_chain()) return false;
    if (!create_command_buffer_pool()) return false;
    if (!command_queue.init(device, swap_chain, queue_family, 0)) return false;
    
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

    VkPhysicalDeviceFeatures device_features = {};
    device_features.geometryShader     = VK_TRUE;
    device_features.tessellationShader = VK_TRUE;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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

        vkGetPhysicalDeviceFeatures(physical_device, &device->features);
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
    create_info.queueFamilyIndex = queue_family;

    CHECK_VK_RESULT(vkCreateCommandPool(device, &create_info, NULL, &command_buffer_pool), "Failed to create command buffer pool");

    logprintf("Vulkan command buffer pool created!\n");

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

VkRenderPass Vulkan_Context::create_simple_render_pass() {
    VkAttachmentDescription attachment_description = {};
    attachment_description.format = swap_chain_surface_format.format;
    attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSubpassDescription subpass_description = {};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = NULL;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &attachment_reference;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &attachment_description;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;

    VkRenderPass render_pass;
    if (vkCreateRenderPass(device, &render_pass_create_info, NULL, &render_pass) != VK_SUCCESS) {
        logprintf("Failed to create simple render pass\n");
        return NULL;
    }

    logprintf("Vulkan render pass created!\n");
    
    return render_pass;
}

bool Vulkan_Context::create_framebuffers(Array <VkFramebuffer> &framebuffers, VkRenderPass render_pass, Platform_Window *window) {
    framebuffers.resize(images.count);

    for (int i = 0; i < framebuffers.count; i++) {
        VkFramebufferCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &image_views[i];
        create_info.width = window->width;
        create_info.height = window->height;
        create_info.layers = 1;

        CHECK_VK_RESULT(vkCreateFramebuffer(device, &create_info, NULL, &framebuffers[i]), "Failed to create vulkan framebuffer");
    }

    logprintf("Vulkan framebuffers created!\n");
    
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

    return true;
}

void Vulkan_Queue::wait_idle() {
    vkQueueWaitIdle(queue);
}
