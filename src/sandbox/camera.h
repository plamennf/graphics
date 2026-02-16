#pragma once

const float GROUND_LEVEL = 50.0f;
const float MOVEMENT_SPEED = 250.0f;
const float MOVEMENT_SPEED_SPRINT_MULTIPLIER = 2.0f;
const float JUMP_HEIGHT = 8.0f;
const float GRAVITY = 9.81f;

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
};

void init_camera(Camera *camera, Vector3 position, float pitch, float yaw, float roll);
void update_camera(Camera *camera, Camera_Type type, float dt);
void fixed_update_camera(Camera *camera, Camera_Type type, float dtd);
Matrix4 get_view_matrix(Camera *camera);
