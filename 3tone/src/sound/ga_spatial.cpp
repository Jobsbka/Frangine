#include "ga_spatial.hpp"
#include <cmath>
#include <algorithm>

namespace arxglue::sound {

GASpatializer::GASpatializer() {
    // Инициализация моторами по умолчанию
    m_listenerMotor[0] = 1.0f;
    m_sourceMotor[0] = 1.0f;
    updateDerivedData();
}

void GASpatializer::setListenerPose(const Motor& motor) {
    m_listenerMotor = motor;
    m_listenerPos = ga::PGA3D_impl::apply_motor_to_point(motor, {0, 0, 0});
    updateDerivedData();
}

void GASpatializer::setListenerPosition(const Point& pos) {
    m_listenerPos = pos;
    Motor translation = ga::PGA3D_impl::motor_from_axis_angle({1,0,0}, 0, pos);
    m_listenerMotor = translation;
    updateDerivedData();
}

void GASpatializer::setListenerOrientation(const Point& forward, const Point& up) {
    // Нормализуем forward и up
    auto normalize = [](Point& p) {
        float len = std::sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]);
        if (len > 1e-6f) {
            p[0] /= len; p[1] /= len; p[2] /= len;
        }
    };
    Point f = forward, u = up;
    normalize(f); normalize(u);
    // Вычисляем right = cross(f, u) и перестраиваем up для ортогональности
    Point r = {
        f[1]*u[2] - f[2]*u[1],
        f[2]*u[0] - f[0]*u[2],
        f[0]*u[1] - f[1]*u[0]
    };
    normalize(r);
    Point newUp = {
        r[1]*f[2] - r[2]*f[1],
        r[2]*f[0] - r[0]*f[2],
        r[0]*f[1] - r[1]*f[0]
    };
    m_listenerForward = f;
    m_listenerUp = newUp;
    m_listenerRight = r;

    // Строим мотор из матрицы поворота
    std::array<float, 16> mat = {
        r[0], newUp[0], -f[0], 0,
        r[1], newUp[1], -f[1], 0,
        r[2], newUp[2], -f[2], 0,
        0, 0, 0, 1
    };
    Motor rotMotor = ga::PGA3D_impl::motor_from_matrix(mat);
    Motor transMotor = ga::PGA3D_impl::motor_from_axis_angle({1,0,0}, 0, m_listenerPos);
    m_listenerMotor = transMotor * rotMotor;
    updateDerivedData();
}

void GASpatializer::setSourcePose(const Motor& motor) {
    m_sourceMotor = motor;
    m_sourcePos = ga::PGA3D_impl::apply_motor_to_point(motor, {0, 0, 0});
}

void GASpatializer::setSourcePosition(const Point& pos) {
    m_sourcePos = pos;
    Motor translation = ga::PGA3D_impl::motor_from_axis_angle({1,0,0}, 0, pos);
    m_sourceMotor = translation;
}

void GASpatializer::updateDerivedData() {
    // Извлекаем ориентацию слушателя из мотора
    Point origin = {0,0,0};
    Point forwardPt = {0,0,-1};
    Point upPt = {0,1,0};
    m_listenerForward = ga::PGA3D_impl::apply_motor_to_point(m_listenerMotor, forwardPt);
    m_listenerForward[0] -= m_listenerPos[0];
    m_listenerForward[1] -= m_listenerPos[1];
    m_listenerForward[2] -= m_listenerPos[2];
    float len = std::sqrt(m_listenerForward[0]*m_listenerForward[0] +
                          m_listenerForward[1]*m_listenerForward[1] +
                          m_listenerForward[2]*m_listenerForward[2]);
    if (len > 1e-6f) {
        m_listenerForward[0] /= len;
        m_listenerForward[1] /= len;
        m_listenerForward[2] /= len;
    }

    Point upWorld = ga::PGA3D_impl::apply_motor_to_point(m_listenerMotor, upPt);
    upWorld[0] -= m_listenerPos[0];
    upWorld[1] -= m_listenerPos[1];
    upWorld[2] -= m_listenerPos[2];
    len = std::sqrt(upWorld[0]*upWorld[0] + upWorld[1]*upWorld[1] + upWorld[2]*upWorld[2]);
    if (len > 1e-6f) {
        upWorld[0] /= len;
        upWorld[1] /= len;
        upWorld[2] /= len;
    }
    m_listenerUp = upWorld;

    // right = cross(forward, up)
    m_listenerRight = {
        m_listenerForward[1]*m_listenerUp[2] - m_listenerForward[2]*m_listenerUp[1],
        m_listenerForward[2]*m_listenerUp[0] - m_listenerForward[0]*m_listenerUp[2],
        m_listenerForward[0]*m_listenerUp[1] - m_listenerForward[1]*m_listenerUp[0]
    };
}

GASpatializer::Point GASpatializer::getRelativeSourcePosition() const {
    Point rel = {
        m_sourcePos[0] - m_listenerPos[0],
        m_sourcePos[1] - m_listenerPos[1],
        m_sourcePos[2] - m_listenerPos[2]
    };
    return rel;
}

void GASpatializer::computeGains(float& leftGain, float& rightGain, float& distanceAttenuation) const {
    Point rel = getRelativeSourcePosition();
    float dist = std::sqrt(rel[0]*rel[0] + rel[1]*rel[1] + rel[2]*rel[2]);
    if (dist < 0.1f) dist = 0.1f; // избегаем бесконечного усиления

    // Затухание по расстоянию (обратный квадрат)
    distanceAttenuation = 1.0f / (1.0f + dist * 0.5f);

    // Нормализуем вектор направления
    Point dir = {rel[0]/dist, rel[1]/dist, rel[2]/dist};

    // Проекция на right и forward слушателя (для панорамирования и фронтального ослабления)
    float dotRight = dir[0]*m_listenerRight[0] + dir[1]*m_listenerRight[1] + dir[2]*m_listenerRight[2];
    float dotForward = dir[0]*m_listenerForward[0] + dir[1]*m_listenerForward[1] + dir[2]*m_listenerForward[2];

    // Панорамирование: -1 (лево) .. 1 (право)
    float pan = std::clamp(dotRight, -1.0f, 1.0f);
    // Ослабление сзади (если источник позади)
    float backAtten = (dotForward < 0) ? 0.7f : 1.0f;

    // Constant power pan
    float panAngle = pan * 0.785398f; // 45 градусов максимум
    leftGain = std::cos(panAngle) * backAtten;
    rightGain = std::sin(panAngle) * backAtten;
}

void GASpatializer::processMonoToStereo(const AudioBuffer& input,
                                        AudioBuffer& outputLeft,
                                        AudioBuffer& outputRight) {
    size_t numFrames = input.getNumFrames();
    if (numFrames == 0) return;

    const float* inData = input.getChannelData(0);
    float* outLeft = outputLeft.getChannelData(0);
    float* outRight = outputRight.getChannelData(0);

    float leftGain, rightGain, distAtt;
    computeGains(leftGain, rightGain, distAtt);

    float totalLeft = leftGain * distAtt;
    float totalRight = rightGain * distAtt;

    for (size_t i = 0; i < numFrames; ++i) {
        float sample = inData[i * input.getSpec().numChannels];
        outLeft[i] = sample * totalLeft;
        outRight[i] = sample * totalRight;
    }
}

} // namespace arxglue::sound