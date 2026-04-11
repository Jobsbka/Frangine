// src/math/ga/versor.hpp
#pragma once

#include "multivector.hpp"
#include <cmath>
#include <algorithm>

namespace arxglue::ga {

template<typename Sig>
class Versor : public Multivector<Sig> {
public:
    using Base = Multivector<Sig>;
    using Base::Base;

    // Явный конструктор из Multivector (необходим для преобразования результата умножения)
    Versor(const Multivector<Sig>& mv) : Base(mv) {}

    // Создание версора из бивектора (экспонента)
    static Versor from_bivector(const Multivector<Sig>& B) {
        float norm = B.norm();
        Versor result;
        if (norm < 1e-6f) {
            result[0] = 1.0f;
        } else {
            float cos_norm = std::cos(norm);
            float sin_norm = std::sin(norm);
            result[0] = cos_norm;
            Multivector<Sig> B_norm = B * (sin_norm / norm);
            for (size_t i = 0; i < Base::size; ++i) {
                if (i != 0) result[i] = B_norm[i];
            }
        }
        return result;
    }

    // Применение версора к мультивектору (сэндвич-произведение)
    Multivector<Sig> apply(const Multivector<Sig>& X) const {
        Versor inv = this->reverse();
        return (*this) * X * inv;
    }

    // Сферическая линейная интерполяция двух версоров
    static Versor slerp(const Versor& v1, const Versor& v2, float t) {
        if (t <= 0.0f) return v1;
        if (t >= 1.0f) return v2;

        // Вычисляем относительный версор
        Versor delta = v1.reverse() * v2;
        Multivector<Sig> log_delta = log_versor(delta);
        Versor scaled = Versor::from_bivector(log_delta * t);
        return v1 * scaled;
    }

    // Обратный версор
    Versor reverse() const {
        return Versor(Base::reverse());
    }

private:
    // Логарифмическое отображение версора (извлечение бивектора)
    static Multivector<Sig> log_versor(const Versor& v) {
        float scalar = v[0];
        float theta = std::acos(std::clamp(scalar, -1.0f, 1.0f));
        if (theta < 1e-6f) {
            Multivector<Sig> result;
            float sin_theta_over_theta = (theta < 1e-6f) ? 1.0f : std::sin(theta) / theta;
            for (size_t i = 1; i < Base::size; ++i) {
                result[i] = v[i] / sin_theta_over_theta;
            }
            return result;
        } else {
            float sin_theta = std::sin(theta);
            Multivector<Sig> B;
            for (size_t i = 1; i < Base::size; ++i) {
                B[i] = v[i] / sin_theta;
            }
            return B * theta;
        }
    }
};

} // namespace arxglue::ga