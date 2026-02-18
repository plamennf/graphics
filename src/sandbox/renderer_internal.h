#pragma once

struct Per_Object_Uniforms {
    Matrix4 world_matrix;
    Vector4 diffuse_color;
    float shininess;
    float padding[3];
};

struct Render_Mesh_Info {
    Gpu_Buffer *vertex_buffer;
    Gpu_Buffer *index_buffer;
    int num_indices;

    Texture *diffuse_texture;
    
    Per_Object_Uniforms uniforms;
};

enum Render_Command_Type {
    RENDER_COMMAND_SET_PER_SCENE_UNIFORMS,
    RENDER_COMMAND_RENDER_MESH,
};

struct Render_Command {
    Render_Command_Type type;

    union {
        Per_Scene_Uniforms per_scene_uniforms;
        Render_Mesh_Info render_mesh_info;
    };
};

const int MAX_RENDER_COMMANDS = 4096;
extern Render_Command *render_commands;
extern int num_render_commands;

static void add_render_command(Render_Command *command) {
    Assert(num_render_commands < MAX_RENDER_COMMANDS);
    render_commands[num_render_commands++] = *command;
}

static inline void add_per_scene_uniforms(Per_Scene_Uniforms *uniforms) {
    Render_Command command;
    command.type               = RENDER_COMMAND_SET_PER_SCENE_UNIFORMS;
    command.per_scene_uniforms = *uniforms;
    add_render_command(&command);
}

static inline void add_render_mesh_info(Render_Mesh_Info *info) {
    Render_Command command;
    command.type             = RENDER_COMMAND_RENDER_MESH;
    command.render_mesh_info = *info;
    add_render_command(&command);
}
