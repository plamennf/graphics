#pragma once

#include "renderer.h"
#include "texture_registry.h"
#include "mesh_registry.h"

const float CAMERA_FOV = 90.0f;
const float CAMERA_Z_NEAR = 0.1f;
const float CAMERA_Z_FAR  = 2000.0f;

const float WORLD_WIDTH  = 100;
const float WORLD_HEIGHT = 40;
const float WORLD_DEPTH  = 60;

struct Global_Variables {
    float mouse_sensitivity = 0.2f;

    Texture_Registry *texture_registry = NULL;
    Mesh_Registry    *mesh_registry = NULL;

    Texture *white_texture = NULL;

    bool enable_imgui = true;
    bool flashlight_on = false;

    float shadow_cascade_splits[MAX_SHADOW_CASCADES] = { 10.0f, 25.0f, 60.0f, CAMERA_Z_FAR };
};

extern Global_Variables globals;
