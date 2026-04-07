#pragma once

#include <array>

namespace arxglue::render {

class Camera {
public:
    Camera();

    void setPerspective(float fovY, float aspect, float nearPlane, float farPlane);
    void lookAt(const std::array<float, 3>& eye,
                const std::array<float, 3>& center,
                const std::array<float, 3>& up);

    const float* getViewMatrix() const { return m_viewMatrix.data(); }
    const float* getProjectionMatrix() const { return m_projMatrix.data(); }

private:
    std::array<float, 16> m_viewMatrix;
    std::array<float, 16> m_projMatrix;

    static void perspective(std::array<float, 16>& mat, float fovY, float aspect, float nearPlane, float farPlane);
    static void lookAt(std::array<float, 16>& mat,
                       const std::array<float, 3>& eye,
                       const std::array<float, 3>& center,
                       const std::array<float, 3>& up);
    static void multiply(std::array<float, 16>& result,
                         const std::array<float, 16>& a,
                         const std::array<float, 16>& b);
};

} // namespace arxglue::render