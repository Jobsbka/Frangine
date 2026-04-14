#pragma once
#include <cmath>
#include <vector>

namespace arxglue::sound::dsp {

enum class BiquadType {
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peak,
    LowShelf,
    HighShelf
};

class BiquadFilter {
public:
    BiquadFilter();
    ~BiquadFilter() = default;

    // Настройка фильтра по типу, частоте, добротности и усилению (dB)
    void setParameters(BiquadType type, double freq, double q, double gainDB, double sampleRate);

    // Обработка одного сэмпла
    float process(float input);

    // Обработка блока сэмплов (in-place)
    void processBlock(float* data, size_t numSamples);

    // Сброс состояния фильтра
    void reset();

    // Прямая установка коэффициентов (для продвинутых пользователей)
    void setCoefficients(double b0, double b1, double b2, double a1, double a2);

private:
    double m_b0, m_b1, m_b2;
    double m_a1, m_a2;
    double m_x1, m_x2; // задержанные входы
    double m_y1, m_y2; // задержанные выходы

    void updateCoefficients();
};

} // namespace arxglue::sound::dsp