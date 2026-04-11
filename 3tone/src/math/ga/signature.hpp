// src/math/ga/signature.hpp
#pragma once

#include <cstddef>
#include <array>
#include <type_traits>

namespace arxglue::ga {

template<int P, int Q, int R>
struct Signature {
    static constexpr int positive = P;
    static constexpr int negative = Q;
    static constexpr int null = R;
    static constexpr int dimension = P + Q + R;
    static constexpr bool is_degenerate = (R > 0);
};

using PGA3D = Signature<3, 0, 1>;
using CGA3D = Signature<4, 1, 0>;
using GA11  = Signature<1, 1, 0>;
using GA10  = Signature<1, 0, 0>;

namespace detail {
    constexpr int count_swaps(unsigned int a, unsigned int b) {
        // Индексы: e1=0, e2=1, e3=2, e0=3
        auto idx = [](unsigned int bit) constexpr -> int {
            if (bit == 1) return 0; // e1
            if (bit == 2) return 1; // e2
            if (bit == 4) return 2; // e3
            return 3;               // e0
        };
        int count = 0;
        unsigned int mask_a = a;
        while (mask_a) {
            unsigned int bit_a = mask_a & (~mask_a + 1);
            int idx_a = idx(bit_a);
            unsigned int mask_b = b;
            while (mask_b) {
                unsigned int bit_b = mask_b & (~mask_b + 1);
                int idx_b = idx(bit_b);
                if (idx_a > idx_b) ++count;
                mask_b ^= bit_b;
            }
            mask_a ^= bit_a;
        }
        return count;
    }

    template<typename Sig, unsigned int A, unsigned int B>
    constexpr int compute_sign() {
        unsigned int common = A & B;
        int swaps = count_swaps(A, B);
        int sign = (swaps % 2 == 0) ? 1 : -1;
        while (common) {
            unsigned int bit = common & (~common + 1);
            int idx = 0;
            unsigned int tmp = bit;
            while (tmp >>= 1) ++idx;
            if (idx < Sig::positive) {
                // положительная сигнатура
            } else if (idx < Sig::positive + Sig::negative) {
                sign = -sign;
            } else {
                sign = 0; // нулевая сигнатура
                break;
            }
            common ^= bit;
        }
        return sign;
    }
}

template<typename Sig, unsigned int A, unsigned int B>
struct BladeProductSign {
    static constexpr int value = detail::compute_sign<Sig, A, B>();
};

} // namespace arxglue::ga