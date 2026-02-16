#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_tangent;
layout(location = 4) in vec3 in_bitangent;

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer Per_Scene_Data {
    mat4 projection_matrix;
    mat4 view_matrix;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer Per_Object_Data {
    mat4 world_matrix;
};

layout(push_constant) uniform Push_Constants {
    Per_Scene_Data per_scene;
    Per_Object_Data per_object;
};

layout (location = 0) out vec2 out_uv;

void main() {
    gl_Position = per_scene.projection_matrix * per_scene.view_matrix * per_object.world_matrix * vec4(in_position, 1.0);

    out_uv = in_uv;
}
