#pragma once

#include <math.h>

const float PI = 3.14159265359f;
const float TAU = 6.28318530718f;

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float square_root(float value) {
#ifdef HAS_SUPPORT_FOR_INTRINSICS
    float result = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
#else
    float result = sqrtf(value);
#endif
    return result;
}

inline s32 round_float32_to_s32(float value) {
#ifdef HAS_SUPPORT_FOR_INTRINSICS
    s32 result = _mm_cvtss_si32(_mm_set_ss(value));
#else
    s32 result = (s32)roundf(value);
#endif
    return result;
}

inline s32 floor_float32_to_s32(float value) {
#ifdef HAS_SUPPORT_FOR_INTRINSICS
    s32 result = _mm_cvtss_si32(_mm_floor_ss(_mm_setzero_ps(), _mm_set_ss(value)));
#else
    s32 result = (s32)floorf(value);
#endif
    return result;
}

inline int absolute_value(int value) {
    if (value < 0) return -value;
    return value;
}

inline float absolute_value(float value) {
    float result = fabsf(value);
    return result;
}

inline float to_radians(float degrees) {
    float radians = degrees * (PI / 180.0f);
    return radians;
}

inline float to_degrees(float radians) {
    float degrees = radians * (180.0f / PI);
    return degrees;
}

inline float move_toward(float a, float b, float amount) {
    if (a > b) {
        a -= amount;
        if (a < b) a = b;
    } else {
        a += amount;
        if (a > b) a = b;
    }

    return a;
}

inline void clamp(int *v, int a, int b) {
    Assert(v);
    if      (*v < a) *v = a;
    else if (*v > b) *v = b;
}

inline void clamp(float *v, float a, float b) {
    Assert(v);
    if      (*v < a) *v = a;
    else if (*v > b) *v = b;
}
