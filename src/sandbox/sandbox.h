#pragma once

struct Shader;

struct Global_Variables {
    Memory_Arena permanent_memory;

    Shader *shader_quad;
};

extern Global_Variables globals;
