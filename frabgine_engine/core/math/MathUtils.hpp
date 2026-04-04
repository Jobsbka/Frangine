#ifndef FRABGINE_CORE_MATH_MATHUTILS_HPP
#define FRABGINE_CORE_MATH_MATHUTILS_HPP

#include <cmath>
#include <algorithm>
#include <random>

namespace frabgine {

constexpr float PI = 3.14159265358979323846f;
constexpr float EPSILON = 0.000001f;

inline float toRadians(float degrees) {
    return degrees * PI / 180.0f;
}

inline float toDegrees(float radians) {
    return radians * 180.0f / PI;
}

inline float clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float smoothstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float smootherstep(float edge0, float edge1, float x) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline bool approximatelyEqual(float a, float b, float epsilon = EPSILON) {
    return std::abs(a - b) < epsilon;
}

inline int sign(float value) {
    return (value > 0.0f) ? 1 : ((value < 0.0f) ? -1 : 0);
}

// Генератор случайных чисел
class Random {
private:
    std::mt19937 generator_;
    
public:
    Random(unsigned int seed = std::random_device{}()) 
        : generator_(seed) {}
    
    void seed(unsigned int seed) {
        generator_.seed(seed);
    }
    
    float range(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(generator_);
    }
    
    int range(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(generator_);
    }
    
    float gaussian(float mean = 0.0f, float stddev = 1.0f) {
        std::normal_distribution<float> dist(mean, stddev);
        return dist(generator_);
    }
};

} // namespace frabgine

#endif // FRABGINE_CORE_MATH_MATHUTILS_HPP
