#pragma once

enum Camera_Type {
    CAMERA_TYPE_FPS,
    CAMERA_TYPE_NOCLIP,
};

struct Camera {
    Vector3 position;
    Vector3 target;
    Vector3 up;

    float pitch;
    float yaw;
    float roll;

    float jump_velocity;
    bool is_on_ground;

    float movement_speed            = 10.0f;
    float shift_movement_multiplier = 1.5f;
    float max_jump_velocity         = 0.5f;
    float gravity                   = 1.0f;
    float head_y                    = 2.0f;
};

void init_camera(Camera *camera, Vector3 position, float pitch, float yaw, float roll);
void update_camera(Camera *camera, Camera_Type type, float dt);
void fixed_update_camera(Camera *camera, Camera_Type type, float dt);
Matrix4 get_view_matrix(Camera *camera);
