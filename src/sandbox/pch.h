#pragma once

#include "corelib/corelib.h"

#include "sandbox.h"

#include <vulkan/vulkan.h>

#define CHECK_VK_RESULT(res, msg) if ((res) != VK_SUCCESS) { logprintf("Error in %s:%d - %s, code %x\n", __FILE__, __LINE__, msg, res); return false; }
