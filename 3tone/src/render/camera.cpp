#include "camera.hpp"
#include <cmath>
#include <cstring>

namespace arxglue::render {

Camera::Camera() {
    // Инициализация единичными матрицами
    for (int i = 0; i < 16; ++i) {
        m_viewMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        m_projMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

void Camera::setPerspective(float fovY, float aspect, float nearPlane, float farPlane) {
    perspective(m_projMatrix, fovY, aspect, nearPlane, farPlane);
}

void Camera::lookAt(const std::array<float, 3>& eye,
                    const std::array<float, 3>& center,
                    const std::array<float, 3>& up) {
    lookAt(m_viewMatrix, eye, center, up);
}

void Camera::perspective(std::array<float, 16>& mat,
                         float fovY, float aspect,
                         float nearPlane, float farPlane) {
    float tanHalfFov = std::tan(fovY * 0.5f);
    mat.fill(0.0f);
    mat[0] = 1.0f / (aspect * tanHalfFov);
    mat[5] = 1.0f / tanHalfFov;
    mat[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    mat[11] = -1.0f;
    mat[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
}

void Camera::lookAt(std::array<float, 16>& mat,
                    const std::array<float, 3>& eye,
                    const std::array<float, 3>& center,
                    const std::array<float, 3>& up) {
    std::array<float, 3> f = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
    float len = std::sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
    f[0] /= len; f[1] /= len; f[2] /= len;

    std::array<float, 3> s = {f[1]*up[2] - f[2]*up[1],
                              f[2]*up[0] - f[0]*up[2],
                              f[0]*up[1] - f[1]*up[0]};
    len = std::sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
    s[0] /= len; s[1] /= len; s[2] /= len;

    std::array<float, 3> u = {s[1]*f[2] - s[2]*f[1],
                              s[2]*f[0] - s[0]*f[2],
                              s[0]*f[1] - s[1]*f[0]};

    mat[0] = s[0]; mat[1] = u[0]; mat[2] = -f[0]; mat[3] = 0.0f;
    mat[4] = s[1]; mat[5] = u[1]; mat[6] = -f[1]; mat[7] = 0.0f;
    mat[8] = s[2]; mat[9] = u[2]; mat[10] = -f[2]; mat[11] = 0.0f;
    mat[12] = -(s[0]*eye[0] + s[1]*eye[1] + s[2]*eye[2]);
    mat[13] = -(u[0]*eye[0] + u[1]*eye[1] + u[2]*eye[2]);
    mat[14] = f[0]*eye[0] + f[1]*eye[1] + f[2]*eye[2];
    mat[15] = 1.0f;
}

void Camera::multiply(std::array<float, 16>& result,
                      const std::array<float, 16>& a,
                      const std::array<float, 16>& b) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result[i*4 + j] = a[i*4 + 0] * b[0*4 + j] +
                              a[i*4 + 1] * b[1*4 + j] +
                              a[i*4 + 2] * b[2*4 + j] +
                              a[i*4 + 3] * b[3*4 + j];
        }
    }
}

} // namespace arxglue::render