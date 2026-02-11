#include "pch.h"
#include "renderer.h"

Global_Variables globals;

int main(int argc, char *argv[]) {
    init_temporary_storage(Kilobytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };

    ma_init(&globals.permanent_memory, Megabytes(4));
    
    Platform_Window *window = platform_window_create(0, 0, "Sandbox");
    Renderer *renderer = renderer_create(RENDER_API_D3D12, window, true);
    defer { renderer_shutdown(renderer); };
    init_immediate_rendering(renderer);

    Sampler_Info static_sampler = {TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT};
    
    Shader_Info shader_info         = {};
    shader_info.render_vertex_type  = RENDER_VERTEX_TYPE_IMMEDIATE;
    shader_info.filepath            = "data/shaders/basic.hlsl";
    shader_info.num_static_samplers = 1;
    shader_info.static_samplers     = &static_sampler;
    globals.shader_quad = renderer_load_shader(renderer, shader_info);

    u8 white_texture_data[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    Texture *texture = renderer_allocate_texture(renderer, 1, 1, TEXTURE_FORMAT_RGBA8, 4, white_texture_data);

    Texture *texture2 = renderer_load_texture(renderer, "data/textures/Warehouse.png");
    
    while (window->is_open) {
        reset_temporary_storage();        
        platform_poll_events();

        if (is_key_down(&window->keyboard, KEY_ALT)) {
            if (is_key_pressed(&window->keyboard, KEY_F4)) {
                window->is_open = false;
            } else if (is_key_pressed(&window->keyboard, KEY_ENTER)) {
                platform_window_toggle_fullscreen(window);
            }
        }
        
        if (is_key_pressed(&window->keyboard, KEY_F11)) {
            platform_window_toggle_fullscreen(window);
        }

        if (is_key_pressed(&window->keyboard, KEY_ESCAPE)) {
            window->is_open = false;
        }

        Per_Scene_Uniforms per_scene_uniforms = {};
        per_scene_uniforms.projection_matrix = make_orthographic(0.0f, (float)window->width, 0.0f, (float)window->height, -1.0f, 1.0f);
        per_scene_uniforms.view_matrix = matrix4_identity();
        renderer_set_per_scene_uniforms(renderer, per_scene_uniforms);

        immediate_begin(renderer);
        immediate_quad(renderer, texture, v2(50, 50), v2(64, 64), v4(1, 0.5f, 0.2f, 1));
        immediate_quad(renderer, texture2, v2(500, 500), v2(128, 128), v4(1, 1, 1, 1));
        immediate_flush(renderer);
        
        renderer_execute_render_commands_and_present(renderer);
        renderer_move_to_next_frame(renderer);
    }
    
    return 0;
}
