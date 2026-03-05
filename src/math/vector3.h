#pragma once

struct Vector3 {
    union {
        struct {
            float x;
            float y;
            float z;
        };
        struct {
            float r;
            float g;
            float b;
        };
        float e[3];
    };

    inline float &operator[](int index) {
        Assert(index >= 0);
        Assert(index < 3);
        return e[index];
    }

    inline float const &operator[](int index) const {
        Assert(index >= 0);
        Assert(index < 3);
        return e[index];
    }
};

#define v3 make_vector3

inline Vector3 make_vector3(float x, float y, float z) {
    Vector3 result;

    result.x = x;
    result.y = y;
    result.z = z;

    return result;
}

inline Vector3 make_vector3(Vector2 xy, float z) {
    Vector3 result;

    result.x = xy.x;
    result.y = xy.y;
    result.z = z;
}

inline Vector3 operator+(Vector3 a, Vector3 b) {
    Vector3 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;

    return result;
}

inline Vector3 operator-(Vector3 a, Vector3 b) {
    Vector3 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;

    return result;
}

inline Vector3 operator*(Vector3 a, float b) {
    Vector3 result;

    result.x = a.x * b;
    result.y = a.y * b;
    result.z = a.z * b;

    return result;
}

inline Vector3 operator*(float a, Vector3 b) {
    Vector3 result;

    result.x = a * b.x;
    result.y = a * b.y;
    result.z = a * b.z;

    return result;
}

inline Vector3 operator/(Vector3 a, float b) {
    Vector3 result;

    float inv_b = 1.0f / b;

    result.x = a.x * inv_b;
    result.y = a.y * inv_b;
    result.z = a.z * inv_b;

    return result;
}

inline Vector3 &operator+=(Vector3 &a, Vector3 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline Vector3 &operator-=(Vector3 &a, Vector3 b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

inline bool operator<(Vector3 a, Vector3 b) {
    return a.x < b.x && a.y < b.y && a.z < b.z;
}

inline bool operator>(Vector3 a, Vector3 b) {
    return a.x > b.x && a.y > b.y && a.z > b.z;
}

inline float length_squared(Vector3 v) {
    return v.x*v.x + v.y*v.y + v.z*v.z;
}

inline float length(Vector3 v) {
    return square_root(length_squared(v));
}
    
inline Vector3 normalize(Vector3 v) {
    float multiplier = 1.0f / length(v);
    v.x *= multiplier;
    v.y *= multiplier;
    v.z *= multiplier;
    return v;
}

inline Vector3 normalize_or_zero(Vector3 v) {
    Vector3 result = {};
    
    float len_sq = length_squared(v);
    if (len_sq > 0.0001f * 0.0001f) {
        float multiplier = 1.0f / square_root(len_sq);
        result.x = v.x * multiplier;
        result.y = v.y * multiplier;
        result.z = v.z * multiplier;
    }

    return result;
}

inline Vector3 cross_product(Vector3 a, Vector3 b) {
    Vector3 result;
    result.x = a.y*b.z - a.z*b.y;
    result.y = a.z*b.x - a.x*b.z;
    result.z = a.x*b.y - a.y*b.x;
    return result;
}

inline float get_barycentric(Vector3 p0, Vector3 p1, Vector3 p2, Vector2 pos) {
    float det = (p1.z - p2.z) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.z - p2.z);
    float l0 = ((p1.z - p2.z) * (pos.x - p2.x) + (p2.x - p1.x) * (pos.y - p2.z)) / det;
    float l1 = ((p2.z - p0.z) * (pos.x - p2.x) + (p0.x - p2.x) * (pos.y - p2.z)) / det;
    float l2 = 1.0f - l0 - l1;
    float result = l0*p0.y + l1*p1.y + l2*p2.y;
    return result;
}

inline Vector3 componentwise_product(Vector3 a, Vector3 b) {
    Vector3 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;

    return result;
}

inline float distance(Vector3 a, Vector3 b) {
    return square_root(Square(a.x-b.x) + Square(a.y-b.y) + Square(a.z-b.z));
}

inline Vector3 lerp(Vector3 a, Vector3 b, float t) {
    return a + t * (b - a);
}
