#include "pch.h"
#include "renderer.h"
#include "mesh.h"
#include "camera.h"

#include <imgui.h>

#include <stdio.h>

#define DO_SPONZA

Global_Variables globals;

static Mesh *mesh;
static Mesh *helmet;
static Mesh *dragon;
static Mesh *playset;
static Mesh *suzanne;
static Mesh *toycar;
static Mesh *vircity;
static Mesh *cube;
static Camera camera;

static Command_Buffer cb;

struct Shadow_Bounding_Box {
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;
};

static Shadow_Bounding_Box shadow_bounding_boxes[MAX_SHADOW_CASCADES];

//static Vector3 directional_light_direction = v3(-0.3f, -1.0f, -0.5f);
//static Vector3 directional_light_direction = v3(0, 0.05f, -1.0f); // Horizon
static Vector3 directional_light_direction = v3(0, -1, 0); // Noon

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

#if 0
static void update_shadow_map_cascade_matrices(Per_Scene_Uniforms *uniforms, Light *directional_light) {
    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        float prev_split = (i == 0) ? CAMERA_Z_NEAR : globals.shadow_cascade_splits[i - 1];
        float curr_split = globals.shadow_cascade_splits[i];

        float min_z = prev_split - camera.position.z;
        float max_z = curr_split - camera.position.z;
        
        Vector3 camera_position = camera.position;
        Vector3 light_direction = normalize_or_zero(directional_light->direction);

        float s = curr_split;
        Matrix4 light_proj = make_orthographic(-WORLD_WIDTH, WORLD_WIDTH, -WORLD_DEPTH, WORLD_DEPTH, min_z, max_z);
        
        Vector3 light_target; // camera_position - (light_direction * 500.0f);
        light_target.x = WORLD_WIDTH * 0.5f;
        light_target.y = WORLD_DEPTH * 0.5f;
        light_target.z = (max_z - min_z) * 0.5f;

        Vector3 light_eye = light_target + light_direction * 500.0f;
        
        Vector3 world_up = (fabsf(light_direction.y) > 0.99f) ? v3(0, 0, 1) : v3(0, 1, 0);
        world_up = v3(0, 1, 0);

        shadow_bounding_boxes[i].min_x = -WORLD_WIDTH;
        shadow_bounding_boxes[i].max_x = +WORLD_WIDTH;

        shadow_bounding_boxes[i].min_y = -WORLD_DEPTH;
        shadow_bounding_boxes[i].max_y = +WORLD_DEPTH;

        shadow_bounding_boxes[i].min_z = min_z;
        shadow_bounding_boxes[i].max_z = max_z;
        
        Matrix4 light_view = make_look_at_matrix(light_eye, light_target, world_up);
        
        uniforms->light_matrix[i]   = transpose(light_proj * light_view);
        uniforms->cascade_splits[i] = { globals.shadow_cascade_splits[i], 0.0f, 0.0f, 0.0f };
    }
}
#else
static void update_shadow_map_cascade_matrices(Per_Scene_Uniforms *uniforms, Light *directional_light) {
    Matrix4 view_matrix = get_view_matrix(&camera);
    Matrix4 inv_view_matrix = inverse(view_matrix);

    Matrix4 light_matrix = make_look_at_matrix(v3(0, 0, 0), directional_light->direction, v3(0, 1, 0));

    float aspect_ratio = (float)platform_window_width / (float)platform_window_height;
    float tan_half_v_fov = tanf(to_radians(CAMERA_FOV * 0.5f));
    float tan_half_h_fov = tan_half_v_fov * aspect_ratio;

    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        float curr_cascade = (i == 0) ? CAMERA_Z_NEAR : globals.shadow_cascade_splits[i - 1];
        float next_cascade = globals.shadow_cascade_splits[i];
        
        float xn = curr_cascade * tan_half_h_fov;
        float xf = next_cascade * tan_half_h_fov;
        float yn = curr_cascade * tan_half_v_fov;
        float yf = next_cascade * tan_half_v_fov;

        Vector4 frustum_corners[] = {
            v4(+xn, +yn, -curr_cascade, 1.0f),
            v4(-xn, +yn, -curr_cascade, 1.0f),
            v4(+xn, -yn, -curr_cascade, 1.0f),
            v4(-xn, -yn, -curr_cascade, 1.0f),

            v4(+xf, +yf, -next_cascade, 1.0f),
            v4(-xf, +yf, -next_cascade, 1.0f),
            v4(+xf, -yf, -next_cascade, 1.0f),
            v4(-xf, -yf, -next_cascade, 1.0f),
        };
        
        Vector4 frustum_corners_l[8];

        Vector3 center_world = v3(0, 0, 0);

        for (int j = 0; j < 8; j++) {
            Vector4 vw = inv_view_matrix * frustum_corners[j];
            center_world += v3(vw.x, vw.y, vw.z);
        }
        center_world = center_world / 8.0f;
        
        Vector3 light_dir = normalize_or_zero(directional_light->direction);
        Vector3 light_eye = center_world - light_dir * 1000.0f;

        Vector3 world_up = (fabsf(light_dir.y) > 0.99f) ? v3(0, 0, 1) : v3(0, 1, 0);
        //world_up = v3(0, 1, 0);
        Matrix4 light_view = make_look_at_matrix(light_eye, center_world, world_up);
        
        float min_x = FLT_MAX, max_x = -FLT_MAX;
        float min_y = FLT_MAX, max_y = -FLT_MAX;
        float min_z = FLT_MAX, max_z = -FLT_MAX;
        
        for (int j = 0; j < 8; j++) {
            Vector4 vw = inv_view_matrix * frustum_corners[j];

            frustum_corners_l[j] = light_view * vw;

            min_x = Min(min_x, frustum_corners_l[j].x);
            min_y = Min(min_y, frustum_corners_l[j].y);
            min_z = Min(min_z, frustum_corners_l[j].z);
            
            max_x = Max(max_x, frustum_corners_l[j].x);
            max_y = Max(max_y, frustum_corners_l[j].y);
            max_z = Max(max_z, frustum_corners_l[j].z);
        }

#if 1
        float world_units_per_texel = (max_x - min_x) / (float)SHADOW_MAP_WIDTH;
        min_x = floorf(min_x / world_units_per_texel) * world_units_per_texel;
        max_x = floorf(max_x / world_units_per_texel) * world_units_per_texel;
        min_y = floorf(min_y / world_units_per_texel) * world_units_per_texel;
        max_y = floorf(max_y / world_units_per_texel) * world_units_per_texel;
#endif

        shadow_bounding_boxes[i].min_x = min_x;
        shadow_bounding_boxes[i].min_y = min_y;
        shadow_bounding_boxes[i].min_z = min_z;

        shadow_bounding_boxes[i].max_x = max_x;
        shadow_bounding_boxes[i].max_y = max_y;
        shadow_bounding_boxes[i].max_z = max_z;
        
        Matrix4 light_proj = make_orthographic(min_x, max_x, min_y, max_y, -max_z, -min_z);
        
        uniforms->light_matrix[i] = transpose(light_proj * light_view);

        Vector4 view = v4(0, 0, camera.position.z - next_cascade, 1.0f);
        Vector4 clip = uniforms->view_matrix * view;
        
        //uniforms->cascade_splits[i] = v4(clip.z, 0.0f, 0.0f, 0.0f);
        uniforms->cascade_splits[i] = v4(globals.shadow_cascade_splits[i], 0.0f, 0.0f, 0.0f);
    }
}
#endif

static void render_scene(Command_Buffer *cb) {
#ifdef DO_SPONZA
    render_mesh(cb, cube, v3(-50, -1, -50), v3(0, 0, 0), v3(100, 1, 100), v4(0, 0, 1, 1));
    float scale = 0.025f;
    render_mesh(cb, mesh, v3(0, 0, 0), v3(0, 0, 0), v3(scale, scale, scale), v4(1, 1, 1, 1));
#else
    render_mesh(cb, cube, v3(-50, -1, -50), v3(0, 0, 0), v3(100, 1, 100), v4(0, 0, 1, 1));

    render_mesh(cb, mesh,    v3(0, 0, 0), v3(0, 0, 0), v3(1, 1, 1), v4(1, 1, 1, 1));
    render_mesh(cb, helmet,  v3(0, 2, -5), v3(0, 0, 0), v3(1, 1, 1), v4(1, 1, 1, 1));
    render_mesh(cb, dragon,  v3(0, 0, -15), v3(90, 0, 0), v3(1, 1, 1), v4(1, 1, 1, 1));
    render_mesh(cb, playset, v3(0, 0, -25), v3(0, 0, 0), v3(5, 5, 5), v4(1, 1, 1, 1));
    render_mesh(cb, suzanne, v3(0, 2, -35), v3(0, 0, 0), v3(1, 1, 1), v4(1, 1, 1, 1));
    render_mesh(cb, toycar,  v3(0, 0, -45), v3(90, 0, 0), v3(0.025f, 0.025f, 0.025f), v4(1, 1, 1, 1));
    render_mesh(cb, vircity, v3(0, 0, -70), v3(0, 0, 0), v3(0.001f, 0.001f, 0.001f), v4(1, 1, 1, 1));
#endif
}

static void draw_one_frame() {
    ZoneScopedN("Set up render commands");
    
    Per_Scene_Uniforms per_scene_uniforms;
    per_scene_uniforms.projection_matrix = transpose(make_perspective((float)platform_window_width / (float)platform_window_height, CAMERA_FOV, CAMERA_Z_NEAR, CAMERA_Z_FAR));
    per_scene_uniforms.view_matrix = transpose(get_view_matrix(&camera));

    per_scene_uniforms.camera_position = camera.position;
    
    Light sun = {};
    sun.type      = LIGHT_TYPE_DIRECTIONAL;
    sun.direction = normalize_or_zero(directional_light_direction);
    sun.color     = v3(1.0f, 0.95f, 0.85f);
    sun.intensity = 1.2f;

    update_shadow_map_cascade_matrices(&per_scene_uniforms, &sun);
    
    Light l0 = {};
    l0.type = LIGHT_TYPE_POINT;
    l0.position = v3(0.0f, 32.0f, 0.0f);
    l0.color = v3(1.0f, 0.85f, 0.7f);
    l0.intensity = 600.0f;
    l0.range = 60.0f;

    Light l1 = {};
    l1.type = LIGHT_TYPE_POINT;
    l1.position = v3(-30.0f, 15.0f, 0.0f);
    l1.color = v3(0.4f, 0.6f, 1.0f);
    l1.intensity = 400.0f;
    l1.range = 45.0f;

    Light l2 = {};
    l2.type = LIGHT_TYPE_POINT;
    l2.position = v3(30.0f, 12.0f, -10.0f);
    l2.color = v3(1.0f, 1.0f, 1.0f);
    l2.intensity = 700.0f;
    l2.range = 40.0f;

    Light l3 = {};
    l3.type = LIGHT_TYPE_POINT;
    l3.position = v3(0.0f, 10.0f, -25.0f);
    l3.color = v3(1.0f, 0.95f, 0.8f);
    l3.intensity = 250.0f;
    l3.range = 50.0f;

    Light l4 = {};
    l4.type = LIGHT_TYPE_POINT;
    l4.position = v3(-15.0f, 10.0f, 20.0f);
    l4.color = v3(0.6f, 0.7f, 1.0f);
    l4.intensity = 350.0f;
    l4.range = 35.0f;

    Light l5 = {};
    l5.type = LIGHT_TYPE_POINT;
    l5.position = v3(20.0f, 18.0f, 10.0f);
    l5.color = v3(1.0f, 0.9f, 0.75f);
    l5.intensity = 400.0f;
    l5.range = 40.0f;

    Light spot_light = {};
    if (globals.flashlight_on) {
        spot_light.type      = LIGHT_TYPE_SPOT;
        spot_light.position  = camera.position;
        spot_light.direction = normalize_or_zero(camera.target);
        spot_light.color     = v3(1.0f, 1.0f, 0.9f);
        spot_light.intensity = 800.0f;
        spot_light.range     = 50.0f;
        spot_light.spot_inner_cone_angle = cosf(to_radians(12.5f));
        spot_light.spot_outer_cone_angle = cosf(to_radians(20.0f));
    }
    
    per_scene_uniforms.lights[0] = sun;
    per_scene_uniforms.lights[1] = l0;
    per_scene_uniforms.lights[2] = l1;
    per_scene_uniforms.lights[3] = l2;
    per_scene_uniforms.lights[4] = l3;
    per_scene_uniforms.lights[5] = l4;
    per_scene_uniforms.lights[6] = l5;
    per_scene_uniforms.lights[7] = spot_light;
    
    //set_per_scene_uniforms(&cb, &per_scene_uniforms);

    set_pipeline_type(&cb, RENDER_PIPELINE_SHADOW);

    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        clear_depth_target(&cb, &shadow_map_targets[i], 1.0f, 0);
        set_render_targets(&cb, 0, NULL, &shadow_map_targets[i]);
        set_viewport(&cb, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
        
        per_scene_uniforms.shadow_cascade_index = i;
        set_per_scene_uniforms(&cb, &per_scene_uniforms);

        render_scene(&cb);
    }
    
    clear_render_target(&cb, &offscreen_render_target, v4(0.2f, 0.5f, 0.8f, 1.0f));
    clear_depth_target(&cb, &offscreen_depth_target, 1.0f, 0);
    set_render_targets(&cb, 1, &offscreen_render_target, &offscreen_depth_target);
    set_viewport(&cb, platform_window_width, platform_window_height);
    
    set_pipeline_type(&cb, RENDER_PIPELINE_MESH);

    render_scene(&cb);

    resolve_render_targets(&cb, &offscreen_render_target, &back_buffer);

    set_texture(&cb, TEXTURE_ALBEDO, NULL);
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

    Memory_Budget memory_info = get_vram_memory();
    ImGui::Text("VRAM: %d/%d", memory_info.used / 1000 / 1000, memory_info.max / 1000 / 1000);
    
    for (int i = 0; i < MAX_SHADOW_CASCADES; i++) {
        ImGui::Text("X: %f to %f", shadow_bounding_boxes[i].min_x, shadow_bounding_boxes[i].max_x);
        ImGui::Text("Y: %f to %f", shadow_bounding_boxes[i].min_y, shadow_bounding_boxes[i].max_y);
        ImGui::Text("Z: %f to %f", shadow_bounding_boxes[i].min_z, shadow_bounding_boxes[i].max_z);
    }
    
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
    ImGui::SliderFloat3("Directional light direction", &directional_light_direction.x, 0.0f, 2.0f);
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
    defer { release_texture(globals.white_texture); };
    
#ifdef DO_SPONZA
    mesh = globals.mesh_registry->find_or_load("Sponza");
    if (!mesh) return 1;
#else
    mesh = globals.mesh_registry->find_or_load("Demon");
    if (!mesh) return 1;
    
    helmet = globals.mesh_registry->find_or_load("DamagedHelmet");
    if (!helmet) return 1;

    dragon = globals.mesh_registry->find_or_load("DragonAttenuation");
    if (!dragon) return 1;

    playset = globals.mesh_registry->find_or_load("PlaysetLightTest");
    if (!playset) return 1;

    suzanne = globals.mesh_registry->find_or_load("Suzanne");
    if (!suzanne) return 1;

    toycar = globals.mesh_registry->find_or_load("ToyCar");
    if (!toycar) return 1;

    vircity = globals.mesh_registry->find_or_load("VirtualCity");
    if (!vircity) return 1;
#endif

    cube = globals.mesh_registry->find_or_load("Cube");
    if (!cube) return 1;

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

        if (is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)) {
            globals.flashlight_on = !globals.flashlight_on;
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

#ifdef RENDER_D3D11
    extern void imgui_shutdown_dx11();
    imgui_shutdown_dx11();
#endif
    
    platform_imgui_shutdown();
    ImGui::DestroyContext();
    
    return 0;
}
