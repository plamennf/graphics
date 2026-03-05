#pragma once

struct Quaternion {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float w = 1.0f;
};

inline Quaternion operator*(Quaternion a, Quaternion b) {
    Quaternion result = {};
        
    result.w = (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z);
    result.x = (a.x * b.w) + (a.w * b.x) + (a.y * b.z) - (a.z * b.y);
    result.y = (a.y * b.w) + (a.w * b.y) + (a.z * b.x) - (a.x * b.z);
    result.z = (a.z * b.w) + (a.w * b.z) + (a.x * b.y) - (a.y * b.x);

    return result;
}

inline Quaternion operator*(Quaternion a, Vector3 b) {
    Quaternion result = {};
        
    result.w = - (a.x * b.x) - (a.y * b.y) - (a.z * b.z);
    result.x =   (a.w * b.x) + (a.y * b.z) - (a.z * b.y);
    result.y =   (a.w * b.y) + (a.z * b.x) - (a.x * b.z);
    result.z =   (a.w * b.z) + (a.x * b.y) - (a.y * b.x);
        
    return result;
}

inline void set_from_axis_and_angle(Quaternion *q, Vector3 v, float angle) {
    float half_angle_radians = (angle * 0.5f) * (PI / 180.0f);

    float sine_half_angle = sinf(half_angle_radians);
    float cosine_half_angle = cosf(half_angle_radians);

    q->x = v.x * sine_half_angle;
    q->y = v.y * sine_half_angle;
    q->z = v.z * sine_half_angle;
    q->w = cosine_half_angle;
}

inline Quaternion conjugate(Quaternion q) {
    Quaternion result;

    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    result.w = q.w;
        
    return result;
}

inline Matrix4 get_rotation_matrix(Quaternion q) {
    Matrix4 m = {};

    float xy = q.x*q.y;
    float xz = q.x*q.z;
    float xw = q.x*q.w;
    float yz = q.y*q.z;
    float yw = q.y*q.w;
    float zw = q.z*q.w;
    float x_sq = q.x*q.x;
    float y_sq = q.y*q.y;
    float z_sq = q.z*q.z;
        
    m._11 = 1 - 2 * (y_sq + z_sq);
    m._21 = 2 * (xy - zw);
    m._31 = 2 * (xz + yw);
    m._41 = 0.0f;

    m._12 = 2 * (xy + zw);
    m._22 = 1.0f - 2.0f * (x_sq + z_sq);
    m._32 = 2 * (yz - xw);
    m._42 = 0.0f;

    m._13 = 2 * (xz - yw);
    m._23 = 2 * (yz + xw);
    m._33 = 1.0f - 2.0f * (x_sq + y_sq);
    m._43 = 0.0f;

    m._14 = 0.0f;
    m._24 = 0.0f;
    m._34 = 0.0f;
    m._44 = 1.0f;

    return m;
}

inline float length_squared(Quaternion q) {
    return q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
}

inline float length(Quaternion q) {
    return square_root(length_squared(q));
}

inline Quaternion normalize_or_zero(Quaternion q) {
    Quaternion result = {};
    
    float len_sq = length_squared(q);
    if (len_sq < 0.001f * 0.001f) result;
    float inv_len = 1.0f / square_root(len_sq);

    result.x = q.x * inv_len;
    result.y = q.y * inv_len;
    result.z = q.z * inv_len;
    result.w = q.w * inv_len;

    return result;
}
