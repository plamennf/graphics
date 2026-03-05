#pragma once

struct Vector2 {
    union {
        struct {
            float x;
            float y;
        };
        struct {
            float r;
            float g;
        };
        float e[2];
    };

    inline float &operator[](int index) {
        Assert(index >= 0);
        Assert(index < 2);
        return e[index];
    }

    inline float const &operator[](int index) const {
        Assert(index >= 0);
        Assert(index < 2);
        return e[index];
    }
};

#define v2 make_vector2

inline Vector2 make_vector2(float x, float y) {
    Vector2 result;

    result.x = x;
    result.y = y;

    return result;
}

inline Vector2 operator+(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return result;
}

inline Vector2 operator-(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return result;
}

inline Vector2 operator*(Vector2 a, float b) {
    Vector2 result;

    result.x = a.x * b;
    result.y = a.y * b;

    return result;
}

inline Vector2 operator*(float a, Vector2 b) {
    Vector2 result;

    result.x = a * b.x;
    result.y = a * b.y;

    return result;
}

inline Vector2 operator/(Vector2 a, float b) {
    Vector2 result;

    float inv_b = 1.0f / b;

    result.x = a.x * inv_b;
    result.y = a.y * inv_b;

    return result;
}

inline Vector2 &operator+=(Vector2 &a, Vector2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

inline Vector2 &operator-=(Vector2 &a, Vector2 b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

inline Vector2 &operator*=(Vector2 &a, float b) {
    a.x *= b;
    a.y *= b;
    return a;
}

inline bool operator<(Vector2 a, Vector2 b) {
    return a.x < b.x && a.y < b.y;
}

inline bool operator>(Vector2 a, Vector2 b) {
    return a.x > b.x && a.y > b.y;
}

inline float length_squared(Vector2 v) {
    return v.x*v.x + v.y*v.y;
}

inline float length(Vector2 v) {
    return square_root(length_squared(v));
}
    
inline Vector2 normalize(Vector2 v) {
    float multiplier = 1.0f / length(v);
    v.x *= multiplier;
    v.y *= multiplier;
    return v;
}

inline Vector2 normalize_or_zero(Vector2 v) {
    Vector2 result = {};
    
    float len_sq = length_squared(v);
    if (len_sq > 0.0001f * 0.0001f) {
        float multiplier = 1.0f / square_root(len_sq);
        result.x = v.x * multiplier;
        result.y = v.y * multiplier;
    }

    return result;
}

inline Vector2 componentwise_product(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;

    return result;
}

inline float dot_product(Vector2 a, Vector2 b) {
    return a.x*b.x + a.y*b.y;
}

inline Vector2 get_vec2(float theta) {
    float ct = cosf(theta);
    float st = sinf(theta);

    return make_vector2(ct, st);
}

inline Vector2 absolute_value(Vector2 v) {
    Vector2 result;

    result.x = fabsf(v.x);
    result.y = fabsf(v.y);

    return result;
}

inline Vector2 rotate(Vector2 v, float theta) {
    float ct = cosf(theta);
    float st = sinf(theta);

    Vector2 result;

    result.x = v.x*ct - v.y*st;
    result.y = v.x*st + v.y*ct;

    return result;
}

inline Vector2 move_toward(Vector2 a, Vector2 b, float amount) {
    Vector2 result;

    result.x = move_toward(a.x, b.x, amount);
    result.y = move_toward(a.y, b.y, amount);

    return result;
}

inline Vector2 minv(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = Min(a.x, b.x);
    result.y = Min(a.y, b.y);

    return result;
}

inline Vector2 maxv(Vector2 a, Vector2 b) {
    Vector2 result;

    result.x = Max(a.x, b.x);
    result.y = Max(a.y, b.y);

    return result;
}

inline Vector2 clampv(Vector2 v, Vector2 a, Vector2 b) {
    return maxv(a, minv(a, b));
}

inline Vector2 lerp(Vector2 a, Vector2 b, float t) {
    return a + t * (b - a);
}

struct Vector2i {
    union {
        struct {
            int x;
            int y;
        };
        int e[2];
    };
    
    inline int &operator[](int index) {
        Assert(index >= 0);
        Assert(index < 2);
        return e[index];
    }

    inline int const &operator[](int index) const {
        Assert(index >= 0);
        Assert(index < 2);
        return e[index];
    }
};

#define make_vector2i v2i

inline Vector2i make_vector2i(int x, int y) {
    Vector2i result;

    result.x = x;
    result.y = y;

    return result;
}

inline Vector2 to_vec2(Vector2i v) {
    Vector2 result;

    result.x = (float)v.x;
    result.y = (float)v.y;

    return result;
}
