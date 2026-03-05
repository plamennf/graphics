#pragma once

struct Vector4 {
    union {
        struct {
            float x;
            float y;
            float z;
            float w;
        };
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        float e[4];
    };

    inline float &operator[](int index) {
        Assert(index >= 0);
        Assert(index < 4);
        return e[index];
    }

    inline float const &operator[](int index) const {
        Assert(index >= 0);
        Assert(index < 4);
        return e[index];
    }
};

#define v4 make_vector4

inline Vector4 make_vector4(float x, float y, float z, float w) {
    Vector4 result;

    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;

    return result;
}

inline Vector4 make_vector4(Vector3 xyz, float w) {
    Vector4 result;

    result.x = xyz.x;
    result.y = xyz.y;
    result.z = xyz.z;
    result.w = w;

    return result;
}

inline Vector4 operator+(Vector4 a, Vector4 b) {
    Vector4 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    result.w = a.w + b.w;

    return result;
}

inline Vector4 operator-(Vector4 a, Vector4 b) {
    Vector4 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    result.w = a.w - b.w;

    return result;
}

inline Vector4 operator*(Vector4 a, float b) {
    Vector4 result;

    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;
    result.w = a.w * b;

    return result;    
}

inline Vector4 operator*(float a, Vector4 b) {
    Vector4 result;

    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;
    result.w = a * b.w;

    return result;    
}

inline Vector4 lerp(Vector4 a, Vector4 b, float t) {
    return a + t * (b - a);
}

inline u32 argb_color(Vector4 color) {
    u32 ir = (u32)(color.x * 255.0f);
    u32 ig = (u32)(color.y * 255.0f);
    u32 ib = (u32)(color.z * 255.0f);
    u32 ia = (u32)(color.w * 255.0f);

    return (ia << 24) | (ir << 16) | (ig << 8) | (ib << 0);
}

inline float linear_to_srgb(float c) {
    return powf(c, 1.0f / 2.2f);
}

inline float length_squared(Vector4 v) {
    return v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w;
}

inline float length(Vector4 v) {
    return square_root(length_squared(v));
}

inline Vector4 normalize_or_zero(Vector4 v) {
    Vector4 result = {};
    
    float len_sq = length_squared(v);
    if (len_sq > 0.0001f * 0.0001f) {
        float multiplier = 1.0f / square_root(len_sq);
        result.x = v.x * multiplier;
        result.y = v.y * multiplier;
        result.z = v.z * multiplier;
        result.w = v.w * multiplier;
    }

    return result;
}
