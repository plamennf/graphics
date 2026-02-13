#pragma once

#include "vulkan_context.h"

struct Mesh {
    u32 vertex_buffer_size = 0;
    Vulkan_Buffer_And_Memory vertex_buffer;
};
