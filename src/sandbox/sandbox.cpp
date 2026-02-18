#include "pch.h"
#include "renderer.h"
#include "mesh.h"

#include <stdio.h>

Global_Variables globals;

static Mesh mesh;

static void draw_one_frame() {
    Per_Scene_Uniforms per_scene_uniforms;
    per_scene_uniforms.projection_matrix = make_perspective((float)platform_window_width / (float)platform_window_height, 90.0f, 0.1f, 1000.0f);
    per_scene_uniforms.view_matrix = matrix4_identity();
    set_per_scene_uniforms(per_scene_uniforms);

    render_mesh(&mesh, v3(0, -2, -5), v3(0, 0, 0), v3(1, 1, 1), v4(1, 1, 1, 1));
}

int main(int argc, char *argv[]) {
    init_temporary_storage(Megabytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    if (!platform_window_create(0, 0, "Sandbox")) return false;
    init_renderer(true);

    if (!load_mesh(&mesh, "data/meshes/Demon.gltf")) return false;
    generate_gpu_data_for_mesh(&mesh);
    
    bool should_show_cursor = true;

    float fixed_update_dt = 1.0f / 60.0f;
    u64 last_time = platform_get_time_in_nanoseconds();
    float accumulated_dt = 0.0f;
    while (platform_window_is_open) {
        reset_temporary_storage();

        u64 now_time = platform_get_time_in_nanoseconds();
        u64 dt_ns = now_time - last_time;
        last_time = now_time;

        float dt = (float)((double)dt_ns / 1000000000.0);
        accumulated_dt += dt;
        
        platform_poll_events();

        if (is_key_down(KEY_ALT)) {
            if (is_key_pressed(KEY_F4)) {
                platform_window_is_open = false;
            } else if (is_key_pressed(KEY_ENTER)) {
                platform_window_toggle_fullscreen();
            }
        }
        
        if (is_key_pressed(KEY_F11)) {
            platform_window_toggle_fullscreen();
        }

        if (is_key_pressed(KEY_ESCAPE)) {
            should_show_cursor = !should_show_cursor;
        }
        
        if (should_show_cursor) {
            platform_show_and_unlock_cursor();
        } else {
            platform_hide_and_lock_cursor();
        }

        if (platform_window_was_resized()) {
            resize_renderer();
        }

        if (!should_show_cursor) {
            //update_camera(&camera, CAMERA_TYPE_FPS, dt);
        }

        while (accumulated_dt >= fixed_update_dt) {
            if (!should_show_cursor) {
                //fixed_update_camera(&camera, CAMERA_TYPE_FPS, fixed_update_dt);
            }
            accumulated_dt -= fixed_update_dt;
        }

        draw_one_frame();
        
        render_frame_and_present(v4(0.2f, 0.5f, 0.8f, 1.0f));
    }
    
    return 0;
}
