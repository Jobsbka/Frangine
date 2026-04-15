// src/math/ga/multivector.hpp
#pragma once

#include "signature.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <type_traits>
#include <ostream>

namespace arxglue::ga {

inline unsigned int popcount(unsigned int x) noexcept {
#if defined(_MSC_VER)
    return __popcnt(x);
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned int>(__builtin_popcount(x));
#else
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    return (((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24;
#endif
}

template<typename Sig>
class Multivector {
public:
    static constexpr size_t size = 1u << Sig::dimension;
    using CoeffArray = std::array<float, size>;

    Multivector() : coeffs_{} {}
    explicit Multivector(const CoeffArray& arr) : coeffs_(arr) {}
    
    float& operator[](size_t blade) { return coeffs_[blade]; }
    const float& operator[](size_t blade) const { return coeffs_[blade]; }

    Multivector& operator+=(const Multivector& rhs) {
        for (size_t i = 0; i < size; ++i) coeffs_[i] += rhs.coeffs_[i];
        return *this;
    }

    Multivector& operator-=(const Multivector& rhs) {
        for (size_t i = 0; i < size; ++i) coeffs_[i] -= rhs.coeffs_[i];
        return *this;
    }

    Multivector& operator*=(float scalar) {
        for (auto& c : coeffs_) c *= scalar;
        return *this;
    }

    Multivector operator+(const Multivector& rhs) const {
        Multivector result(*this);
        result += rhs;
        return result;
    }

    Multivector operator-(const Multivector& rhs) const {
        Multivector result(*this);
        result -= rhs;
        return result;
    }

    Multivector operator*(float scalar) const {
        Multivector result(*this);
        result *= scalar;
        return result;
    }

    friend Multivector operator*(float scalar, const Multivector& mv) {
        return mv * scalar;
    }

    Multivector geometric_product(const Multivector& rhs) const {
        Multivector result;
        const auto& table = product_table<Sig>;
        for (size_t a = 0; a < size; ++a) {
            float ca = coeffs_[a];
            if (ca == 0.0f) continue;
            for (size_t b = 0; b < size; ++b) {
                float cb = rhs.coeffs_[b];
                if (cb == 0.0f) continue;
                int sign = table.sign[a][b];
                if (sign == 0) continue;
                size_t c = table.blade[a][b];
                result.coeffs_[c] += sign * ca * cb;
            }
        }
        return result;
    }

    Multivector operator*(const Multivector& rhs) const {
        return geometric_product(rhs);
    }

    Multivector wedge(const Multivector& rhs) const {
        Multivector result;
        for (size_t a = 0; a < size; ++a) {
            float ca = coeffs_[a];
            if (ca == 0.0f) continue;
            for (size_t b = 0; b < size; ++b) {
                float cb = rhs.coeffs_[b];
                if (cb == 0.0f) continue;
                if ((a & b) != 0) continue;
                int sign = (popcount(static_cast<unsigned int>(a & b)) % 2 == 0) ? 1 : -1;
                size_t c = a | b;
                result.coeffs_[c] += sign * ca * cb;
            }
        }
        return result;
    }

    Multivector operator^(const Multivector& rhs) const {
        return wedge(rhs);
    }

    Multivector left_contraction(const Multivector& rhs) const {
        Multivector result;
        const auto& table = product_table<Sig>;
        for (size_t a = 0; a < size; ++a) {
            float ca = coeffs_[a];
            if (ca == 0.0f) continue;
            for (size_t b = 0; b < size; ++b) {
                float cb = rhs.coeffs_[b];
                if (cb == 0.0f) continue;
                if ((a & b) != a) continue;
                int sign = table.sign[a][b];
                if (sign == 0) continue;
                size_t c = a ^ b;
                result.coeffs_[c] += sign * ca * cb;
            }
        }
        return result;
    }

    Multivector reverse() const {
        Multivector result;
        for (size_t i = 0; i < size; ++i) {
            int grade = popcount(static_cast<unsigned int>(i));
            int sign = ((grade * (grade - 1)) / 2) % 2 == 0 ? 1 : -1;
            result.coeffs_[i] = sign * coeffs_[i];
        }
        return result;
    }

    Multivector involute() const {
        Multivector result;
        for (size_t i = 0; i < size; ++i) {
            int grade = popcount(static_cast<unsigned int>(i));
            result.coeffs_[i] = (grade % 2 == 0) ? coeffs_[i] : -coeffs_[i];
        }
        return result;
    }

    Multivector conjugate() const {
        return reverse().involute();
    }

    Multivector dual() const {
        static const Multivector I = pseudoscalar();
        return left_contraction(I.reverse());
    }

    static Multivector pseudoscalar() {
        Multivector I;
        I.coeffs_[size - 1] = 1.0f;
        return I;
    }

    float norm() const {
        Multivector rev = reverse();
        Multivector prod = geometric_product(rev);
        return std::sqrt(std::abs(prod.coeffs_[0]));
    }

    Multivector normalized() const {
        float n = norm();
        if (n > 1e-6f) {
            return (*this) * (1.0f / n);
        }
        return Multivector{};
    }

    template<int Grade>
    Multivector extract_grade() const {
        static_assert(Grade >= 0 && Grade <= Sig::dimension, "Invalid grade");
        Multivector result;
        for (size_t i = 0; i < size; ++i) {
            if (popcount(static_cast<unsigned int>(i)) == Grade) {
                result.coeffs_[i] = coeffs_[i];
            }
        }
        return result;
    }

    float scalar() const { return coeffs_[0]; }

    std::array<float, Sig::dimension> vector_part() const {
        std::array<float, Sig::dimension> result{};
        for (size_t i = 0; i < Sig::dimension; ++i) {
            size_t blade = 1u << i;
            result[i] = coeffs_[blade];
        }
        return result;
    }

    bool operator==(const Multivector& rhs) const {
        for (size_t i = 0; i < size; ++i) {
            if (std::abs(coeffs_[i] - rhs.coeffs_[i]) > 1e-6f) return false;
        }
        return true;
    }

    bool operator!=(const Multivector& rhs) const {
        return !(*this == rhs);
    }

    const CoeffArray& coeffs() const { return coeffs_; }

private:
    CoeffArray coeffs_;
};

template<typename Sig>
std::ostream& operator<<(std::ostream& os, const Multivector<Sig>& mv) {
    os << "Multivector[";
    for (size_t i = 0; i < mv.size; ++i) {
        if (mv[i] != 0.0f) {
            os << " " << i << ":" << mv[i];
        }
    }
    os << " ]";
    return os;
}

// Базовая таблица (будет специализирована)
template<typename Sig>
struct ProductTable {
    static constexpr size_t size = 1u << Sig::dimension;
    std::array<std::array<int, size>, size> sign{};
    std::array<std::array<size_t, size>, size> blade{};

    constexpr ProductTable() {
        for (size_t a = 0; a < size; ++a) {
            for (size_t b = 0; b < size; ++b) {
                blade[a][b] = a ^ b;
                sign[a][b] = 0;
            }
        }
    }
};

// Полная корректная таблица знаков для PGA3D (индексы 0..15)
// Индексы: 0=1, 1=e1, 2=e2, 3=e12, 4=e3, 5=e13, 6=e23, 7=e123,
// 8=e0, 9=e01, 10=e02, 11=e012, 12=e03, 13=e013, 14=e023, 15=e0123
inline constexpr std::array<std::array<int, 16>, 16> PGA3D_SIGN_TABLE = {{
    { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1},
    { 1,  1,  1,  1,  1,  1,  1,  1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1, -1,  1, -1,  1, -1,  1, -1, -1,  1, -1,  1, -1,  1, -1,  1},
    { 1, -1,  1, -1, -1,  1, -1,  1,  1, -1,  1, -1, -1,  1, -1,  1},
    { 1, -1, -1,  1,  1, -1, -1,  1, -1,  1,  1, -1, -1,  1,  1, -1},
    { 1, -1, -1,  1, -1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1,  1},
    { 1,  1, -1, -1,  1,  1, -1, -1, -1, -1,  1,  1, -1, -1,  1,  1},
    { 1,  1,  1,  1,  1,  1,  1,  1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1, -1,  1, -1,  1, -1,  1, -1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1, -1,  1, -1, -1,  1, -1,  1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1, -1, -1,  1,  1, -1, -1,  1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1, -1, -1,  1, -1,  1,  1, -1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1,  1, -1, -1,  1,  1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0}
}};

template<>
struct ProductTable<PGA3D> {
    static constexpr size_t size = 16;
    std::array<std::array<int, size>, size> sign;
    std::array<std::array<size_t, size>, size> blade;

    constexpr ProductTable() : sign(PGA3D_SIGN_TABLE), blade{} {
        for (size_t a = 0; a < size; ++a)
            for (size_t b = 0; b < size; ++b)
                blade[a][b] = a ^ b;
    }
};

template<typename Sig>
inline constexpr ProductTable<Sig> product_table{};

} // namespace arxglue::ga