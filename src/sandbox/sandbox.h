#pragma once

#include "renderer.h"

struct Global_Variables {
    float mouse_sensitivity = 0.2f;

    Shader shader_basic;
};

extern Global_Variables globals;
