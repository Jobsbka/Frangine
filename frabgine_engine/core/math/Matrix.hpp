#ifndef FRABGINE_CORE_MATH_MATRIX_HPP
#define FRABGINE_CORE_MATH_MATRIX_HPP

#include "Vector.hpp"
#include <array>
#include <cmath>

namespace frabgine {

class Matrix44 {
private:
    std::array<std::array<float, 4>, 4> data_;

public:
    Matrix44() {
        // Инициализация единичной матрицей
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                data_[i][j] = (i == j) ? 1.0f : 0.0f;
    }
    
    float& operator()(int row, int col) { return data_[row][col]; }
    const float& operator()(int row, int col) const { return data_[row][col]; }
    
    float* data() { return &data_[0][0]; }
    const float* data() const { return &data_[0][0]; }
    
    static Matrix44 identity() {
        return Matrix44();
    }
    
    static Matrix44 translation(const Vector3fNamed& t) {
        Matrix44 m;
        m(0, 3) = t.x();
        m(1, 3) = t.y();
        m(2, 3) = t.z();
        return m;
    }
    
    static Matrix44 scale(const Vector3fNamed& s) {
        Matrix44 m;
        m(0, 0) = s.x();
        m(1, 1) = s.y();
        m(2, 2) = s.z();
        return m;
    }
    
    static Matrix44 rotationX(float angle) {
        Matrix44 m;
        float c = std::cos(angle);
        float s = std::sin(angle);
        m(1, 1) = c;
        m(1, 2) = -s;
        m(2, 1) = s;
        m(2, 2) = c;
        return m;
    }
    
    static Matrix44 rotationY(float angle) {
        Matrix44 m;
        float c = std::cos(angle);
        float s = std::sin(angle);
        m(0, 0) = c;
        m(0, 2) = s;
        m(2, 0) = -s;
        m(2, 2) = c;
        return m;
    }
    
    static Matrix44 rotationZ(float angle) {
        Matrix44 m;
        float c = std::cos(angle);
        float s = std::sin(angle);
        m(0, 0) = c;
        m(0, 1) = -s;
        m(1, 0) = s;
        m(1, 1) = c;
        return m;
    }
    
    static Matrix44 perspective(float fov, float aspect, float near, float far) {
        Matrix44 m;
        float tanHalfFov = std::tan(fov / 2.0f);
        
        m(0, 0) = 1.0f / (aspect * tanHalfFov);
        m(1, 1) = 1.0f / tanHalfFov;
        m(2, 2) = -(far + near) / (far - near);
        m(2, 3) = -(2.0f * far * near) / (far - near);
        m(3, 2) = -1.0f;
        m(3, 3) = 0.0f;
        
        return m;
    }
    
    static Matrix44 lookAt(const Vector3fNamed& eye, const Vector3fNamed& center, const Vector3fNamed& up) {
        Vector3fNamed f = (center - eye).normalize();
        Vector3fNamed s = f.cross(up).normalize();
        Vector3fNamed u = s.cross(f);
        
        Matrix44 m;
        m(0, 0) = s.x();
        m(0, 1) = s.y();
        m(0, 2) = s.z();
        m(1, 0) = u.x();
        m(1, 1) = u.y();
        m(1, 2) = u.z();
        m(2, 0) = -f.x();
        m(2, 1) = -f.y();
        m(2, 2) = -f.z();
        m(0, 3) = -s.dot(eye);
        m(1, 3) = -u.dot(eye);
        m(2, 3) = f.dot(eye);
        
        return m;
    }
    
    Matrix44 operator*(const Matrix44& other) const {
        Matrix44 result;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                result(i, j) = 0;
                for (int k = 0; k < 4; ++k)
                    result(i, j) += data_[i][k] * other(k, j);
            }
        return result;
    }
    
    Vector3fNamed transformPoint(const Vector3fNamed& p) const {
        float w = data_[3][0] * p.x() + data_[3][1] * p.y() + data_[3][2] * p.z() + data_[3][3];
        return Vector3fNamed(
            (data_[0][0] * p.x() + data_[0][1] * p.y() + data_[0][2] * p.z() + data_[0][3]) / w,
            (data_[1][0] * p.x() + data_[1][1] * p.y() + data_[1][2] * p.z() + data_[1][3]) / w,
            (data_[2][0] * p.x() + data_[2][1] * p.y() + data_[2][2] * p.z() + data_[2][3]) / w
        );
    }
    
    Vector3fNamed transformVector(const Vector3fNamed& v) const {
        return Vector3fNamed(
            data_[0][0] * v.x() + data_[0][1] * v.y() + data_[0][2] * v.z(),
            data_[1][0] * v.x() + data_[1][1] * v.y() + data_[1][2] * v.z(),
            data_[2][0] * v.x() + data_[2][1] * v.y() + data_[2][2] * v.z()
        );
    }
};

} // namespace frabgine

#endif // FRABGINE_CORE_MATH_MATRIX_HPP
