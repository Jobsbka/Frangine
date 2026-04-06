#pragma once
#include "../math/math.hpp"

// Камера от третьего лица для Space Invaders-style игры
class Camera {
public:
    Camera();
    
    // Обновить камеру на основе ввода и дельты времени
    void update(const struct InputState& input, float dt);
    
    // Получить матрицу вида (view matrix)
    sMatrix34 getViewMatrix() const;
    
    // Получить матрицу проекции
    sMatrix44 getProjectionMatrix(float aspectRatio) const;
    
    // Позиция камеры
    const sVector31& getPosition() const { return position; }
    
    // Направление взгляда (куда смотрит камера)
    sVector30 getForward() const;
    
private:
    sVector31 position;
    float yaw;    // угол поворота вокруг оси Y (в радианах)
    float pitch;  // угол наклона вверх/вниз
    float distance;  // расстояние до цели слежения
    
    // Целевая точка (за которой следит камера)
    sVector31 target;
    
    // Константы
    static constexpr float MOVE_SPEED = 10.0f;
    static constexpr float ROTATE_SPEED = 2.0f;
    static constexpr float DEFAULT_DISTANCE = 30.0f;
};
