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

    Sampler_Info static_sampler = {TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT};
    
    Shader_Info shader_info         = {};
    shader_info.render_vertex_type  = RENDER_VERTEX_TYPE_IMMEDIATE;
    shader_info.filepath            = "data/shaders/basic.hlsl";
    shader_info.num_static_samplers = 1;
    shader_info.static_samplers     = &static_sampler;
    Shader *shader = renderer_load_shader(renderer, shader_info);

    Immediate_Vertex quad_vertices[] = {
        { { -0.5f, -0.5f }, { 1.0f, 0.5f, 0.2f, 1.0f }, { 0.0f, 0.0f } },
        { { +0.5f, -0.5f }, { 1.0f, 0.5f, 0.2f, 1.0f }, { 1.0f, 0.0f } },
        { { +0.5f, +0.5f }, { 1.0f, 0.5f, 0.2f, 1.0f }, { 1.0f, 1.0f } },

        //{ { -0.5f, -0.5f }, { 1.0f, 0.5f, 0.2f, 1.0f }, { 0.0f, 0.0f } },
        //{ { +0.5f, +0.5f }, { 1.0f, 0.5f, 0.2f, 1.0f }, { 1.0f, 1.0f } },
        { { -0.5f, +0.5f }, { 1.0f, 0.5f, 0.2f, 1.0f }, { 0.0f, 1.0f } },
    };

    u32 quad_indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    Gpu_Buffer *vertex_buffer = renderer_allocate_buffer(renderer, GPU_BUFFER_TYPE_VERTEX_BUFFER, sizeof(quad_vertices), sizeof(Immediate_Vertex), quad_vertices);
    Gpu_Buffer *index_buffer = renderer_allocate_buffer(renderer, GPU_BUFFER_TYPE_INDEX_BUFFER, sizeof(quad_indices), sizeof(u32), quad_indices);

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

        Draw_Item_Info draw_item_info;
        draw_item_info.shader        = shader;
        draw_item_info.texture       = texture2;
        draw_item_info.vertex_buffer = vertex_buffer;
        draw_item_info.index_buffer  = index_buffer;
        draw_item_info.num_indices   = ArrayCount(quad_indices);
        draw_item_info.first_index   = 0;
        renderer_draw_item(renderer, draw_item_info);
        
        renderer_execute_render_commands_and_present(renderer);
        renderer_move_to_next_frame(renderer);
    }
    
    return 0;
}
