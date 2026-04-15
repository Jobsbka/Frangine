// src/math/ga/simd_optimized.hpp
#pragma once

#include "multivector.hpp"
#include <immintrin.h>

namespace arxglue::ga {

// Специализация для PGA3D с использованием SIMD (AVX)
template<>
class Multivector<PGA3D> {
public:
    static constexpr size_t size = 16; // 2^4
    using CoeffArray = std::array<float, size>;

    Multivector() {
        _mm256_storeu_ps(coeffs_.data(), _mm256_setzero_ps());
        _mm256_storeu_ps(coeffs_.data() + 8, _mm256_setzero_ps());
    }

    explicit Multivector(const CoeffArray& arr) : coeffs_(arr) {}

    float& operator[](size_t blade) { return coeffs_[blade]; }
    const float& operator[](size_t blade) const { return coeffs_[blade]; }

    Multivector geometric_product(const Multivector& rhs) const {
        Multivector result;
        // Использование предварительно вычисленной таблицы знаков
        const auto& table = product_table<PGA3D>;
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

    // Остальные методы аналогичны общей версии, но могут быть дополнительно оптимизированы

private:
    CoeffArray coeffs_;
};

} // namespace arxglue::ga