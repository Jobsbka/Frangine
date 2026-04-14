#pragma once
#include "../core/math/ga/conversions.hpp"
#include "../core/math/ga/versor.hpp"
#include "../core/math/ga/multivector.hpp"
#include "audio_buffer.hpp"
#include <array>
#include <vector>

namespace arxglue::sound {

class GASpatializer {
public:
    using Motor = ga::Versor<ga::PGA3D>;
    using Point = std::array<float, 3>;

    GASpatializer();
    ~GASpatializer() = default;

    void setListenerPose(const Motor& motor);
    void setListenerPosition(const Point& pos);
    void setListenerOrientation(const Point& forward, const Point& up);

    void setSourcePose(const Motor& motor);
    void setSourcePosition(const Point& pos);

    // Обработка моно-входа в стерео-выход с бинауральными подсказками
    void processMonoToStereo(const AudioBuffer& input,
                             AudioBuffer& outputLeft,
                             AudioBuffer& outputRight);

    // Установка скорости звука (м/с), по умолчанию 343.0
    void setSpeedOfSound(float speed) { m_speedOfSound = speed; }

    // Включение/выключение доплеровского сдвига (пока не реализовано, задел)
    void setDopplerEnabled(bool enabled) { m_dopplerEnabled = enabled; }

private:
    Motor m_listenerMotor;
    Motor m_sourceMotor;
    Point m_listenerPos = {0, 0, 0};
    Point m_sourcePos = {0, 0, 0};
    float m_speedOfSound = 343.0f;
    bool m_dopplerEnabled = false;

    // Внутренние методы
    void updateDerivedData();
    Point m_listenerForward = {0, 0, -1};
    Point m_listenerUp = {0, 1, 0};
    Point m_listenerRight = {1, 0, 0};

    // Расчёт относительного положения источника
    Point getRelativeSourcePosition() const;

    // Простой панорамировщик на основе направления и расстояния
    void computeGains(float& leftGain, float& rightGain, float& distanceAttenuation) const;
};

} // namespace arxglue::sound