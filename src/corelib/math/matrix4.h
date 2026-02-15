#pragma once

struct Matrix4 {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float e[4][4];
        float le[16];
    };
};

inline Matrix4 matrix4_identity() {
    Matrix4 result = {};

    result._11 = 1.0f;
    result._22 = 1.0f;
    result._33 = 1.0f;
    result._44 = 1.0f;

    return result;
}

inline Matrix4 operator*(Matrix4 a, Matrix4 b) {
    Matrix4 result;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            float sum = 0.0f;
            for (int e = 0; e < 4; e++) {
                sum += a.e[row][e] * b.e[e][col];
            }
            result.e[row][col] = sum;
        }
    }
    return result;
}

inline Matrix4 transpose(Matrix4 m) {
    Matrix4 result;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            result.e[row][col] = m.e[col][row];
        }
    }
    return result;
}

inline Matrix4 inverse(Matrix4 m) {
    Matrix4 inv;
    float det;
    int i;

    inv.le[0] = m.le[5]  * m.le[10] * m.le[15] - 
             m.le[5]  * m.le[11] * m.le[14] - 
             m.le[9]  * m.le[6]  * m.le[15] + 
             m.le[9]  * m.le[7]  * m.le[14] +
             m.le[13] * m.le[6]  * m.le[11] - 
             m.le[13] * m.le[7]  * m.le[10];

    inv.le[4] = -m.le[4]  * m.le[10] * m.le[15] + 
              m.le[4]  * m.le[11] * m.le[14] + 
              m.le[8]  * m.le[6]  * m.le[15] - 
              m.le[8]  * m.le[7]  * m.le[14] - 
              m.le[12] * m.le[6]  * m.le[11] + 
              m.le[12] * m.le[7]  * m.le[10];

    inv.le[8] = m.le[4]  * m.le[9] * m.le[15] - 
             m.le[4]  * m.le[11] * m.le[13] - 
             m.le[8]  * m.le[5] * m.le[15] + 
             m.le[8]  * m.le[7] * m.le[13] + 
             m.le[12] * m.le[5] * m.le[11] - 
             m.le[12] * m.le[7] * m.le[9];

    inv.le[12] = -m.le[4]  * m.le[9] * m.le[14] + 
               m.le[4]  * m.le[10] * m.le[13] +
               m.le[8]  * m.le[5] * m.le[14] - 
               m.le[8]  * m.le[6] * m.le[13] - 
               m.le[12] * m.le[5] * m.le[10] + 
               m.le[12] * m.le[6] * m.le[9];

    inv.le[1] = -m.le[1]  * m.le[10] * m.le[15] + 
              m.le[1]  * m.le[11] * m.le[14] + 
              m.le[9]  * m.le[2] * m.le[15] - 
              m.le[9]  * m.le[3] * m.le[14] - 
              m.le[13] * m.le[2] * m.le[11] + 
              m.le[13] * m.le[3] * m.le[10];

    inv.le[5] = m.le[0]  * m.le[10] * m.le[15] - 
             m.le[0]  * m.le[11] * m.le[14] - 
             m.le[8]  * m.le[2] * m.le[15] + 
             m.le[8]  * m.le[3] * m.le[14] + 
             m.le[12] * m.le[2] * m.le[11] - 
             m.le[12] * m.le[3] * m.le[10];

    inv.le[9] = -m.le[0]  * m.le[9] * m.le[15] + 
              m.le[0]  * m.le[11] * m.le[13] + 
              m.le[8]  * m.le[1] * m.le[15] - 
              m.le[8]  * m.le[3] * m.le[13] - 
              m.le[12] * m.le[1] * m.le[11] + 
              m.le[12] * m.le[3] * m.le[9];

    inv.le[13] = m.le[0]  * m.le[9] * m.le[14] - 
              m.le[0]  * m.le[10] * m.le[13] - 
              m.le[8]  * m.le[1] * m.le[14] + 
              m.le[8]  * m.le[2] * m.le[13] + 
              m.le[12] * m.le[1] * m.le[10] - 
              m.le[12] * m.le[2] * m.le[9];

    inv.le[2] = m.le[1]  * m.le[6] * m.le[15] - 
             m.le[1]  * m.le[7] * m.le[14] - 
             m.le[5]  * m.le[2] * m.le[15] + 
             m.le[5]  * m.le[3] * m.le[14] + 
             m.le[13] * m.le[2] * m.le[7] - 
             m.le[13] * m.le[3] * m.le[6];

    inv.le[6] = -m.le[0]  * m.le[6] * m.le[15] + 
              m.le[0]  * m.le[7] * m.le[14] + 
              m.le[4]  * m.le[2] * m.le[15] - 
              m.le[4]  * m.le[3] * m.le[14] - 
              m.le[12] * m.le[2] * m.le[7] + 
              m.le[12] * m.le[3] * m.le[6];

    inv.le[10] = m.le[0]  * m.le[5] * m.le[15] - 
              m.le[0]  * m.le[7] * m.le[13] - 
              m.le[4]  * m.le[1] * m.le[15] + 
              m.le[4]  * m.le[3] * m.le[13] + 
              m.le[12] * m.le[1] * m.le[7] - 
              m.le[12] * m.le[3] * m.le[5];

    inv.le[14] = -m.le[0]  * m.le[5] * m.le[14] + 
               m.le[0]  * m.le[6] * m.le[13] + 
               m.le[4]  * m.le[1] * m.le[14] - 
               m.le[4]  * m.le[2] * m.le[13] - 
               m.le[12] * m.le[1] * m.le[6] + 
               m.le[12] * m.le[2] * m.le[5];

    inv.le[3] = -m.le[1] * m.le[6] * m.le[11] + 
              m.le[1] * m.le[7] * m.le[10] + 
              m.le[5] * m.le[2] * m.le[11] - 
              m.le[5] * m.le[3] * m.le[10] - 
              m.le[9] * m.le[2] * m.le[7] + 
              m.le[9] * m.le[3] * m.le[6];

    inv.le[7] = m.le[0] * m.le[6] * m.le[11] - 
             m.le[0] * m.le[7] * m.le[10] - 
             m.le[4] * m.le[2] * m.le[11] + 
             m.le[4] * m.le[3] * m.le[10] + 
             m.le[8] * m.le[2] * m.le[7] - 
             m.le[8] * m.le[3] * m.le[6];

    inv.le[11] = -m.le[0] * m.le[5] * m.le[11] + 
               m.le[0] * m.le[7] * m.le[9] + 
               m.le[4] * m.le[1] * m.le[11] - 
               m.le[4] * m.le[3] * m.le[9] - 
               m.le[8] * m.le[1] * m.le[7] + 
               m.le[8] * m.le[3] * m.le[5];

    inv.le[15] = m.le[0] * m.le[5] * m.le[10] - 
              m.le[0] * m.le[6] * m.le[9] - 
              m.le[4] * m.le[1] * m.le[10] + 
              m.le[4] * m.le[2] * m.le[9] + 
              m.le[8] * m.le[1] * m.le[6] - 
              m.le[8] * m.le[2] * m.le[5];

    det = m.le[0] * inv.le[0] + m.le[1] * inv.le[4] + m.le[2] * inv.le[8] + m.le[3] * inv.le[12];

    if (det == 0)
        return inv;

    det = 1.0f / det;

    for (i = 0; i < 16; i++)
        inv.le[i] = inv.le[i] * det;

    return inv;
}

inline Matrix4 make_perspective(float aspect_ratio, float fov_in_degrees, float z_near, float z_far) {
    float y_scale = (1.0f / tanf(fov_in_degrees * 0.5f * (PI / 180.0f))) * aspect_ratio;
    float x_scale = y_scale / aspect_ratio;
    float frustum_length = z_far - z_near;

    Matrix4 m = {};
    m._11 = x_scale;
    m._22 = y_scale;
    //m._33 = -((z_far + z_near) / frustum_length);
    m._33 = z_far / (z_near - z_far);
    m._43 = -1.0f;
    m._34 = -((2 * z_near * z_far) / frustum_length);
    m._44 = 0.0f;
    return m;
}

inline Matrix4 make_x_rotation(float t) {
    auto m = matrix4_identity();

    float ct = cosf(t);
    float st = sinf(t);

    m._22 = ct;
    m._23 = -st;
    m._32 = st;
    m._33 = ct;

    return m;
}

inline Matrix4 make_y_rotation(float t) {
    auto m = matrix4_identity();

    float ct = cosf(t);
    float st = sinf(t);

    m._11 = ct;
    m._31 = -st;
    m._13 = st;
    m._33 = ct;

    return m;
}

inline Matrix4 make_z_rotation(float t) {
    auto m = matrix4_identity();

    float ct = cosf(t);
    float st = sinf(t);

    m._11 = ct;
    m._12 = -st;
    m._21 = st;
    m._22 = ct;

    return m;
}

inline Matrix4 make_look_at_matrix(Vector3 pos, Vector3 target, Vector3 world_up) {
    Vector3 z_axis = normalize_or_zero(pos - target);
    Vector3 x_axis = normalize_or_zero(cross_product(normalize_or_zero(world_up), z_axis));
    Vector3 y_axis = cross_product(z_axis, x_axis);

    Matrix4 translation = matrix4_identity();
    translation._14 = -pos.x;
    translation._24 = -pos.y;
    translation._34 = -pos.z;

    Matrix4 rotation = matrix4_identity();
    rotation._11 = x_axis.x;
    rotation._12 = x_axis.y;
    rotation._13 = x_axis.z;
    rotation._21 = y_axis.x;
    rotation._22 = y_axis.y;
    rotation._23 = y_axis.z;
    rotation._31 = z_axis.x;
    rotation._32 = z_axis.y;
    rotation._33 = z_axis.z;

    return rotation * translation;
}

inline Matrix4 make_transformation_matrix(Vector3 position, Vector3 rotation, Vector3 scale) {
    Matrix4 m = matrix4_identity();

    m._14 = position.x;
    m._24 = position.y;
    m._34 = position.z;

    m._11 = scale.x;
    m._22 = scale.y;
    m._33 = scale.z;

    Matrix4 rx = make_x_rotation(rotation.x * (PI / 180.0f));
    Matrix4 ry = make_y_rotation(rotation.y * (PI / 180.0f));
    Matrix4 rz = make_z_rotation(rotation.z * (PI / 180.0f));

    return m * rx * ry * rz;
}

inline Matrix4 make_transformation_matrix(Vector3 position, Vector3 rotation, float scale) {
    return make_transformation_matrix(position, rotation, make_vector3(scale, scale, scale));
}

inline Matrix4 make_orthographic(float l, float r, float b, float t, float n, float f) {
    Matrix4 result = matrix4_identity();

    result._11 = 2.0f/(r-l);
    result._22 = 2.0f/(t-b);
    result._33 = -2.0f/(f-n);
    result._14 = -((r+l)/(r-l));
    result._24 = -((t+b)/(t-b));
    result._34 = -((f+n)/(f-n));
    
    return result;
}
