#pragma once

#include "defines.h"
#include "platform_detection.h"
#include "cstrutils.h"
#include "my_string.h"
#include "logging.h"
#include "memory_arena.h"
#include "temporary_storage.h"
#include "array.h"
#include "hash_table.h"

#include "math/geometry.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/rectangle.h"

#include "key_codes.h"
#include "keyboard.h"
#include "mouse.h"
#include "platform.h"

#include "gl_funcs.h"

#include "renderer.h"
#include "texture_registry.h"
#include "mesh_registry.h"
#include "mesh.h"

#include "sandbox.h"

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN
#define FrameMark
#endif
