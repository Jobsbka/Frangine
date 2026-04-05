#ifndef FRABGINE_CORE_MATH_VECTOR_HPP
#define FRABGINE_CORE_MATH_VECTOR_HPP

#include <cmath>
#include <array>
#include <type_traits>
#include <cstddef>

namespace frabgine {

template<typename T, size_t N>
class VectorBase {
protected:
    std::array<T, N> data_;

public:
    VectorBase() : data_{} {}
    
    VectorBase(std::initializer_list<T> init) {
        size_t i = 0;
        for (const auto& val : init) {
            if (i < N) data_[i++] = val;
        }
    }
    
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    
    T* data() { return data_.data(); }
    const T* data() const { return data_.data(); }
    
    constexpr size_t size() const { return N; }
    
    VectorBase operator+(const VectorBase& other) const {
        VectorBase result;
        for (size_t i = 0; i < N; ++i) result.data_[i] = data_[i] + other.data_[i];
        return result;
    }
    
    VectorBase operator-(const VectorBase& other) const {
        VectorBase result;
        for (size_t i = 0; i < N; ++i) result.data_[i] = data_[i] - other.data_[i];
        return result;
    }
    
    VectorBase operator*(T scalar) const {
        VectorBase result;
        for (size_t i = 0; i < N; ++i) result.data_[i] = data_[i] * scalar;
        return result;
    }
    
    friend VectorBase operator*(T scalar, const VectorBase& vec) {
        return vec * scalar;
    }
    
    VectorBase operator/(T scalar) const {
        VectorBase result;
        for (size_t i = 0; i < N; ++i) result.data_[i] = data_[i] / scalar;
        return result;
    }
    
    VectorBase& operator+=(const VectorBase& other) {
        for (size_t i = 0; i < N; ++i) data_[i] += other.data_[i];
        return *this;
    }
    
    VectorBase& operator-=(const VectorBase& other) {
        for (size_t i = 0; i < N; ++i) data_[i] -= other.data_[i];
        return *this;
    }
    
    VectorBase& operator*=(T scalar) {
        for (size_t i = 0; i < N; ++i) data_[i] *= scalar;
        return *this;
    }
    
    VectorBase& operator/=(T scalar) {
        for (size_t i = 0; i < N; ++i) data_[i] /= scalar;
        return *this;
    }
    
    bool operator==(const VectorBase& other) const {
        for (size_t i = 0; i < N; ++i) if (data_[i] != other.data_[i]) return false;
        return true;
    }
    
    bool operator!=(const VectorBase& other) const {
        return !(*this == other);
    }
    
    VectorBase operator-() const {
        VectorBase result;
        for (size_t i = 0; i < N; ++i) result.data_[i] = -data_[i];
        return result;
    }
    
    T lengthSquared() const {
        T sum = T{};
        for (size_t i = 0; i < N; ++i) sum += data_[i] * data_[i];
        return sum;
    }
    
    T length() const {
        return std::sqrt(lengthSquared());
    }
    
    VectorBase normalize() const {
        T len = length();
        if (len > T{}) return *this / len;
        return *this;
    }
    
    T dot(const VectorBase& other) const {
        T sum = T{};
        for (size_t i = 0; i < N; ++i) sum += data_[i] * other.data_[i];
        return sum;
    }
};

using Vector2f = VectorBase<float, 2>;
using Vector3f = VectorBase<float, 3>;
using Vector4f = VectorBase<float, 4>;

using Vector2d = VectorBase<double, 2>;
using Vector3d = VectorBase<double, 3>;
using Vector4d = VectorBase<double, 4>;

using Vector2i = VectorBase<int, 2>;
using Vector3i = VectorBase<int, 3>;
using Vector4i = VectorBase<int, 4>;

// Специализация для Vector3f с именованными компонентами
class Vector3fNamed : public VectorBase<float, 3> {
public:
    Vector3fNamed() : VectorBase<float, 3>() {}
    Vector3fNamed(float x, float y, float z) { 
        data_[0] = x; data_[1] = y; data_[2] = z; 
    }
    
    float& x() { return data_[0]; }
    float& y() { return data_[1]; }
    float& z() { return data_[2]; }
    
    float x() const { return data_[0]; }
    float y() const { return data_[1]; }
    float z() const { return data_[2]; }
    
    float length() const {
        return std::sqrt(data_[0]*data_[0] + data_[1]*data_[1] + data_[2]*data_[2]);
    }
    
    float lengthSquared() const {
        return data_[0]*data_[0] + data_[1]*data_[1] + data_[2]*data_[2];
    }
    
    Vector3fNamed normalize() const {
        float len = length();
        if (len > 0.0f) return *this / len;
        return *this;
    }
    
    float dot(const Vector3fNamed& other) const {
        return data_[0]*other.data_[0] + data_[1]*other.data_[1] + data_[2]*other.data_[2];
    }
    
    Vector3fNamed cross(const Vector3fNamed& other) const {
        return Vector3fNamed(
            data_[1]*other.data_[2] - data_[2]*other.data_[1],
            data_[2]*other.data_[0] - data_[0]*other.data_[2],
            data_[0]*other.data_[1] - data_[1]*other.data_[0]
        );
    }
};

} // namespace frabgine

#endif // FRABGINE_CORE_MATH_VECTOR_HPP
