#include "camera.hpp"
#include "input_handler.hpp"
#include <cmath>

Camera::Camera() 
    : position(0, 10, -DEFAULT_DISTANCE), 
      yaw(0), 
      pitch(-0.3f), 
      distance(DEFAULT_DISTANCE),
      target(0, 0, 0) {
}

void Camera::update(const InputState& input, float dt) {
    // Движение камеры влево/вправо (стрейф)
    if (input.left) {
        position.x -= MOVE_SPEED * dt;
        target.x -= MOVE_SPEED * dt;
    }
    if (input.right) {
        position.x += MOVE_SPEED * dt;
        target.x += MOVE_SPEED * dt;
    }
    
    // Движение вперёд/назад
    if (input.forward) {
        position.z += MOVE_SPEED * dt;
        target.z += MOVE_SPEED * dt;
    }
    if (input.backward) {
        position.z -= MOVE_SPEED * dt;
        target.z -= MOVE_SPEED * dt;
    }
    
    // Движение вверх/вниз
    if (input.up) {
        position.y += MOVE_SPEED * dt;
        target.y += MOVE_SPEED * dt;
    }
    if (input.down) {
        position.y -= MOVE_SPEED * dt;
        target.y -= MOVE_SPEED * dt;
    }
}

sMatrix34 Camera::getViewMatrix() const {
    // Вычисляем направление взгляда из углов yaw и pitch
    sVector30 forward(
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    );
    forward.Normalize();
    
    // Вычисляем позицию камеры на основе расстояния до цели
    sVector31 desiredPosition = target + forward * (-distance);
    
    // Вектор "вверх" (мировой)
    sVector30 worldUp(0, 1, 0);
    
    // Вычисляем правый вектор
    sVector30 right = forward % worldUp;
    right.Normalize();
    
    // Пересчитываем настоящий up вектор
    sVector30 up = right % forward;
    up.Normalize();
    
    // Строим матрицу вида (lookAt)
    sMatrix34 view;
    view.i = right;
    view.j = up;
    view.k = forward;
    view.l = position;
    
    return view;
}

sMatrix44 Camera::getProjectionMatrix(float aspectRatio) const {
    return sMatrix44::Perspective(1.0472f, aspectRatio, 0.1f, 1000.0f); // FOV ~60 градусов
}

sVector30 Camera::getForward() const {
    return sVector30(
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    );
}
