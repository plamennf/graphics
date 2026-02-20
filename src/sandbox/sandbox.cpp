#include "pch.h"
#include "renderer.h"
#include "mesh.h"
#include "camera.h"

#include <imgui.h>

#include <stdio.h>

#define DO_SPONZA

Global_Variables globals;

static Mesh *mesh;
static Mesh *cube;
static Camera camera;

static Command_Buffer cb;

static void imgui_init() {
    float main_scale = platform_imgui_get_scale();
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;  
    
    platform_imgui_init();

#ifdef RENDER_D3D11
    extern void imgui_init_dx11();
    imgui_init_dx11();
#endif
}

static void imgui_begin_frame() {
    ZoneScopedN("ImGui Begin Frame");
    
#ifdef RENDER_D3D11
    extern void imgui_begin_frame_dx11();
    imgui_begin_frame_dx11();
#endif

    platform_imgui_begin_frame();

    ImGui::NewFrame();
}

static void imgui_end_frame() {
    ZoneScopedN("ImGui End Frame");
    
    ImGui::Render();

#ifdef RENDER_D3D11
    extern void imgui_end_frame_dx11();
    imgui_end_frame_dx11();
#endif
}

static void draw_one_frame() {
    ZoneScopedN("Set up render commands");

    clear_render_target(&cb, &offscreen_render_target, v4(0.2f, 0.5f, 0.8f, 1.0f));
    clear_depth_target(&cb, &offscreen_depth_target, 1.0f, 0);
    set_render_targets(&cb, 1, &offscreen_render_target, &offscreen_depth_target);
    set_viewport(&cb, platform_window_width, platform_window_height);

    set_pipeline_type(&cb, RENDER_PIPELINE_MESH);
    
    Per_Scene_Uniforms per_scene_uniforms;
    per_scene_uniforms.projection_matrix = transpose(make_perspective((float)platform_window_width / (float)platform_window_height, 90.0f, 0.1f, 2000.0f));
    per_scene_uniforms.view_matrix = transpose(get_view_matrix(&camera));

    per_scene_uniforms.camera_position = camera.position;

    Light center_light = {};
    center_light.type = LIGHT_TYPE_POINT;
    center_light.position = { 0.0f, 12.0f, 0.0f };
    center_light.color = { 1.0f, 0.85f, 0.7f };
    center_light.intensity = 120.0f;
    center_light.range = 25.0f;

    Light blue_light = {};
    blue_light.type = LIGHT_TYPE_POINT;
    blue_light.position = { -10.0f, 8.0f, 5.0f };
    blue_light.color = { 0.4f, 0.6f, 1.0f };   // cool blue
    blue_light.intensity = 90.0f;
    blue_light.range = 18.0f;

    Light strong_light = {};
    strong_light.type = LIGHT_TYPE_POINT;
    strong_light.position = { 8.0f, 6.0f, -5.0f };
    strong_light.color = { 1.0f, 1.0f, 1.0f };
    strong_light.intensity = 250.0f;
    strong_light.range = 15.0f;

    Light fill_light = {};
    fill_light.type = LIGHT_TYPE_POINT;
    fill_light.position = { 0.0f, 5.0f, -12.0f };
    fill_light.color = { 1.0f, 0.95f, 0.8f };
    fill_light.intensity = 40.0f;
    fill_light.range = 30.0f;

    per_scene_uniforms.lights[0] = center_light;
    per_scene_uniforms.lights[1] = blue_light;
    per_scene_uniforms.lights[2] = strong_light;
    per_scene_uniforms.lights[3] = fill_light;
    
    set_per_scene_uniforms(&cb, &per_scene_uniforms);

#ifdef DO_SPONZA
    float scale = 0.025f;
    render_mesh(&cb, mesh, v3(0, 0, 0), v3(0, 0, 0), v3(scale, scale, scale), v4(1, 1, 1, 1));
#else
    render_mesh(&cb, cube, v3(-200, -3, -200), v3(0, 0, 0), v3(400, 1, 400), v4(1, 1, 1, 1));
    render_mesh(&cb, mesh, v3(0, -2, -5), v3(0, 0, 0), v3(1, 1, 1), v4(1, 1, 1, 1));
#endif

    resolve_render_targets(&cb, &offscreen_render_target, &back_buffer);
}

static void draw_imgui_stuff(float dt) {
    ZoneScopedN("Draw ImGui Stuff");
    
    static float current_dt = 1.0f;
    static int frame_counter = 0;

    frame_counter++;
    if (frame_counter > 30) {
        current_dt = dt;
        frame_counter = 0;
    }
    
    ImGui::Begin("Frame stats");
    ImGui::Text("FPS: %d", (int)(1.0f / current_dt));
    ImGui::Text("Frame time: %.2fms", current_dt * 1000.0f);
    ImGui::End();

    ImGui::Begin("Sponza");
    ImGui::BeginGroup();
    if (ImGui::TreeNode("Submeshes"))
    {
        for (int i = 0; i < mesh->num_submeshes; ++i) {
            Submesh *submesh   = &mesh->submeshes[i];
            Material *material = &submesh->material;

            ImGui::PushID(i);

            char tree_node_name[64];
            snprintf(tree_node_name, sizeof(tree_node_name), "Submesh %d", i);
            if (ImGui::TreeNode(tree_node_name)) {
                ImGui::Text("Vertices: %d", submesh->num_vertices);
                ImGui::Text("Indices: %d", submesh->num_indices);

                ImGui::Separator();
                ImGui::Text("Material");

                // Texture names
                ImGui::Text("Albedo: %s", 
                            material->albedo_texture_name ? material->albedo_texture_name : "None");

                ImGui::Text("Normal: %s", 
                            material->normal_texture_name ? material->normal_texture_name : "None");

                ImGui::Text("Metallic/Roughness: %s", 
                            material->metallic_roughness_texture_name ? material->metallic_roughness_texture_name : "None");

                ImGui::Text("AO: %s", 
                            material->ao_texture_name ? material->ao_texture_name : "None");

                ImGui::Separator();

                ImGui::ColorEdit4("Diffuse Color", (float *)&material->diffuse_color);
                ImGui::SliderFloat("Shininess", &material->shininess, 1.0f, 256.0f);

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
    }
    ImGui::EndGroup();
    ImGui::End();

    ImGui::Begin("Camera");
    ImGui::SliderFloat("Movement speed", &camera.movement_speed, 0.1f, 30.0f);
    ImGui::SliderFloat("Shift movement multiplier", &camera.shift_movement_multiplier, 1.0f, 5.0f);
    ImGui::SliderFloat("Max jump velocity", &camera.max_jump_velocity, 0.1f, 2.0f);
    ImGui::SliderFloat("Gravity", &camera.gravity, 1.0f, 10.0f);
    ImGui::SliderFloat("Head y", &camera.head_y, 0.0f, 10.0f);
    ImGui::End();
}

int main(int argc, char *argv[]) {
    init_temporary_storage(Megabytes(1));
    
    init_logging();
    defer { shutdown_logging(); };

    platform_init();
    defer { platform_shutdown(); };
    
    if (!platform_window_create(0, 0, "Sandbox")) return false;
    init_renderer(false);
    imgui_init();

    if (!init_command_buffer(&cb)) return false;
    
    globals.texture_registry = new Texture_Registry();
    globals.mesh_registry    = new Mesh_Registry();

    globals.white_texture = new Texture();
    u8 white_texture_data[4] = { 255, 255, 255, 255 };
    if (!create_texture(globals.white_texture, 1, 1, TEXTURE_FORMAT_RGBA8, white_texture_data)) return 1;
    
#ifdef DO_SPONZA
    mesh = globals.mesh_registry->find_or_load("Sponza");
    if (!mesh) return 1;
#else
    mesh = globals.mesh_registry->find_or_load("Demon");
    if (!mesh) return 1;

    cube = globals.mesh_registry->find_or_load("Cube");
    if (!cube) return 1;
#endif

    platform_window_toggle_fullscreen();
    
    bool should_show_cursor = false;

    init_camera(&camera, v3(0, 2, 0), 0, 0, 0);
    
    float fixed_update_dt = 1.0f / 60.0f;
    u64 last_time = platform_get_time_in_nanoseconds();
    float accumulated_dt = 0.0f;
    while (platform_window_is_open) {
        ZoneScopedN("One frame");
        
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

        if (is_key_pressed(KEY_U)) {
            globals.enable_imgui = !globals.enable_imgui;
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
            update_camera(&camera, CAMERA_TYPE_FPS, dt);
        }

        while (accumulated_dt >= fixed_update_dt) {
            if (!should_show_cursor) {
                fixed_update_camera(&camera, CAMERA_TYPE_FPS, fixed_update_dt);
            }
            accumulated_dt -= fixed_update_dt;
        }

        {
            ZoneScopedN("Render one whole frame");
            
            draw_one_frame();
        
            render_frame(1, &cb);

            set_render_targets(&immediate_cb, 1, &back_buffer, NULL);
            
            if (globals.enable_imgui) {
                imgui_begin_frame();
                draw_imgui_stuff(dt);
                imgui_end_frame();
            }
        }

        {
            ZoneScopedN("Swap buffers");
            swap_buffers();
        }

        FrameMark;
    }
    
    return 0;
}
