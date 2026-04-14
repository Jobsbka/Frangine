#include "biquad.hpp"
#include <stdexcept>
#define _USE_MATH_DEFINES
#include <math.h>

namespace arxglue::sound::dsp {

BiquadFilter::BiquadFilter()
    : m_b0(1.0), m_b1(0.0), m_b2(0.0)
    , m_a1(0.0), m_a2(0.0)
    , m_x1(0.0), m_x2(0.0)
    , m_y1(0.0), m_y2(0.0)
{}

void BiquadFilter::setParameters(BiquadType type, double freq, double q, double gainDB, double sampleRate) {
    if (freq <= 0.0 || freq >= sampleRate * 0.5) {
        throw std::invalid_argument("BiquadFilter: frequency out of range");
    }
    if (q <= 0.0) {
        throw std::invalid_argument("BiquadFilter: Q must be positive");
    }

    const double omega = 2.0 * M_PI * freq / sampleRate;
    const double sn = std::sin(omega);
    const double cs = std::cos(omega);
    const double alpha = sn / (2.0 * q);
    const double A = std::pow(10.0, gainDB / 40.0);

    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    double a0 = 0.0, a1 = 0.0, a2 = 0.0;

    switch (type) {
    case BiquadType::LowPass:
        b0 = (1.0 - cs) * 0.5;
        b1 = 1.0 - cs;
        b2 = (1.0 - cs) * 0.5;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case BiquadType::HighPass:
        b0 = (1.0 + cs) * 0.5;
        b1 = -(1.0 + cs);
        b2 = (1.0 + cs) * 0.5;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case BiquadType::BandPass:
        b0 = alpha;
        b1 = 0.0;
        b2 = -alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case BiquadType::Notch:
        b0 = 1.0;
        b1 = -2.0 * cs;
        b2 = 1.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case BiquadType::Peak:
        b0 = 1.0 + alpha * A;
        b1 = -2.0 * cs;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha / A;
        break;
    case BiquadType::LowShelf:
        {
            double beta = std::sqrt(A) * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * cs + beta);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
            b2 = A * ((A + 1.0) - (A - 1.0) * cs - beta);
            a0 = (A + 1.0) + (A - 1.0) * cs + beta;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
            a2 = (A + 1.0) + (A - 1.0) * cs - beta;
        }
        break;
    case BiquadType::HighShelf:
        {
            double beta = std::sqrt(A) * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * cs + beta);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
            b2 = A * ((A + 1.0) + (A - 1.0) * cs - beta);
            a0 = (A + 1.0) - (A - 1.0) * cs + beta;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
            a2 = (A + 1.0) - (A - 1.0) * cs - beta;
        }
        break;
    }

    // Нормализация коэффициентов
    m_b0 = b0 / a0;
    m_b1 = b1 / a0;
    m_b2 = b2 / a0;
    m_a1 = a1 / a0;
    m_a2 = a2 / a0;
}

void BiquadFilter::setCoefficients(double b0, double b1, double b2, double a1, double a2) {
    m_b0 = b0;
    m_b1 = b1;
    m_b2 = b2;
    m_a1 = a1;
    m_a2 = a2;
}

float BiquadFilter::process(float input) {
    double output = m_b0 * input + m_b1 * m_x1 + m_b2 * m_x2
                    - m_a1 * m_y1 - m_a2 * m_y2;

    m_x2 = m_x1;
    m_x1 = input;
    m_y2 = m_y1;
    m_y1 = output;

    return static_cast<float>(output);
}

void BiquadFilter::processBlock(float* data, size_t numSamples) {
    for (size_t i = 0; i < numSamples; ++i) {
        data[i] = process(data[i]);
    }
}

void BiquadFilter::reset() {
    m_x1 = m_x2 = 0.0;
    m_y1 = m_y2 = 0.0;
}

} // namespace arxglue::sound::dsp