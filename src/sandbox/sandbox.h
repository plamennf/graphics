#pragma once

#include "renderer.h"
#include "texture_registry.h"
#include "mesh_registry.h"

struct Global_Variables {
    float mouse_sensitivity = 0.2f;

    Texture_Registry *texture_registry = NULL;
    Mesh_Registry    *mesh_registry = NULL;

    Texture *white_texture = NULL;

    bool enable_imgui = true;
};

extern Global_Variables globals;
