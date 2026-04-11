// src/math/ga/conversions.hpp
#pragma once

#include "multivector.hpp"
#include "versor.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace arxglue::ga {

// ==================== PGA3D ====================
namespace PGA3D_impl {
    using Sig = PGA3D;
    using MV = Multivector<Sig>;
    using VS = Versor<Sig>;

    inline MV point(float x, float y, float z) {
        MV p;
        p[1u << 0] = x;
        p[1u << 1] = y;
        p[1u << 2] = z;
        p[1u << 3] = 1.0f;
        return p;
    }

    inline std::array<float, 3> extract_point(const MV& p) {
        float w = p[1u << 3];
        if (std::abs(w) < 1e-6f) return {0.0f, 0.0f, 0.0f};
        return { p[1u << 0] / w, p[1u << 1] / w, p[1u << 2] / w };
    }

    inline MV vector(float x, float y, float z) {
        MV v;
        v[1u << 0] = x;
        v[1u << 1] = y;
        v[1u << 2] = z;
        return v;
    }

    inline std::array<float, 3> extract_vector(const MV& v) {
        return { v[1u << 0], v[1u << 1], v[1u << 2] };
    }

    inline MV plane(float a, float b, float c, float d) {
        MV pl;
        pl[1u << 0] = a;
        pl[1u << 1] = b;
        pl[1u << 2] = c;
        pl[1u << 3] = d;
        return pl;
    }

    inline MV rotation_bivector(const std::array<float, 3>& axis, float angle) {
        float len = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        if (len < 1e-6f) return MV{};
        std::array<float, 3> n = {axis[0]/len, axis[1]/len, axis[2]/len};
        MV B;
        B[(1u << 0) | (1u << 1)] = -n[2] * angle * 0.5f; // e12
        B[(1u << 1) | (1u << 2)] = -n[0] * angle * 0.5f; // e23
        B[(1u << 2) | (1u << 0)] = -n[1] * angle * 0.5f; // e31
        return B;
    }

    inline VS motor_from_axis_angle(const std::array<float, 3>& axis, float angle,
                                     const std::array<float, 3>& translation = {0.0f, 0.0f, 0.0f}) {
        float len = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        if (len < 1e-6f) {
            VS T;
            T[0] = 1.0f;
            T[(1u << 3) | (1u << 0)] = -translation[0] * 0.5f;
            T[(1u << 3) | (1u << 1)] = -translation[1] * 0.5f;
            T[(1u << 3) | (1u << 2)] = -translation[2] * 0.5f;
            return T;
        }
        std::array<float, 3> n = {axis[0]/len, axis[1]/len, axis[2]/len};
        MV B = rotation_bivector(axis, angle);
        VS R = VS::from_bivector(B);

        if (translation[0] == 0.0f && translation[1] == 0.0f && translation[2] == 0.0f) {
            return R;
        }

        VS T;
        T[0] = 1.0f;
        T[(1u << 3) | (1u << 0)] = -translation[0] * 0.5f;
        T[(1u << 3) | (1u << 1)] = -translation[1] * 0.5f;
        T[(1u << 3) | (1u << 2)] = -translation[2] * 0.5f;
        return R * T;
    }

    inline VS motor_from_matrix(const std::array<float, 16>& mat) {
        std::array<float, 3> translation = { mat[12], mat[13], mat[14] };
        float trace = mat[0] + mat[5] + mat[10];
        float angle = std::acos(std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f));
        std::array<float, 3> axis;
        if (angle < 1e-6f) {
            axis = {1.0f, 0.0f, 0.0f};
            angle = 0.0f;
        } else {
            float s = 1.0f / (2.0f * std::sin(angle));
            axis[0] = (mat[6] - mat[9]) * s;
            axis[1] = (mat[8] - mat[2]) * s;
            axis[2] = (mat[1] - mat[4]) * s;
        }
        return motor_from_axis_angle(axis, angle, translation);
    }

    // Явное применение мотора к точке (разделение вращения и переноса)
    inline std::array<float, 3> apply_motor_to_point(const VS& motor, const std::array<float, 3>& pt) {
        // Извлекаем перенос: t = -2 * (e0^e1, e0^e2, e0^e3)
        std::array<float, 3> t = {
            -2.0f * motor[(1u<<3)|(1u<<0)],
            -2.0f * motor[(1u<<3)|(1u<<1)],
            -2.0f * motor[(1u<<3)|(1u<<2)]
        };
        
        // Извлекаем ротор (чётная часть без e0)
        float scalar = motor[0];
        float b12 = motor[(1u<<0)|(1u<<1)]; // e12
        float b23 = motor[(1u<<1)|(1u<<2)]; // e23
        float b31 = motor[(1u<<2)|(1u<<0)]; // e31
        
        // Нормализуем ротор
        float norm = std::sqrt(scalar*scalar + b12*b12 + b23*b23 + b31*b31);
        if (norm > 1e-6f) {
            scalar /= norm;
            b12 /= norm;
            b23 /= norm;
            b31 /= norm;
        }
        
        // Применяем вращение (кватернион: w=scalar, x=b23, y=b31, z=b12)
        float x = pt[0], y = pt[1], z = pt[2];
        float qw = scalar, qx = b23, qy = b31, qz = b12;
        
        float tx = 2.0f * (qy*z - qz*y);
        float ty = 2.0f * (qz*x - qx*z);
        float tz = 2.0f * (qx*y - qy*x);
        float rx = x + qw*tx + (qy*tz - qz*ty);
        float ry = y + qw*ty + (qz*tx - qx*tz);
        float rz = z + qw*tz + (qx*ty - qy*tx);
        
        return {rx + t[0], ry + t[1], rz + t[2]};
    }

    inline std::array<float, 16> motor_to_matrix(const VS& motor) {
        auto t = apply_motor_to_point(motor, {0,0,0});
        auto c0 = apply_motor_to_point(motor, {1,0,0});
        auto c1 = apply_motor_to_point(motor, {0,1,0});
        auto c2 = apply_motor_to_point(motor, {0,0,1});
        
        std::array<float, 16> mat{};
        mat[0] = c0[0] - t[0]; mat[4] = c1[0] - t[0]; mat[8] = c2[0] - t[0]; mat[12] = t[0];
        mat[1] = c0[1] - t[1]; mat[5] = c1[1] - t[1]; mat[9] = c2[1] - t[1]; mat[13] = t[1];
        mat[2] = c0[2] - t[2]; mat[6] = c1[2] - t[2]; mat[10] = c2[2] - t[2]; mat[14] = t[2];
        mat[3] = 0.0f; mat[7] = 0.0f; mat[11] = 0.0f; mat[15] = 1.0f;
        return mat;
    }

    inline VS motor_from_quaternion(float x, float y, float z, float w) {
        MV B;
        B[(1u << 0) | (1u << 1)] = -z;
        B[(1u << 1) | (1u << 2)] = -x;
        B[(1u << 2) | (1u << 0)] = -y;
        float angle = 2.0f * std::acos(w);
        float sin_half = std::sqrt(1.0f - w*w);
        if (sin_half > 1e-6f) {
            B = B * (angle * 0.5f / sin_half);
        }
        return VS::from_bivector(B);
    }

    inline std::array<float, 4> motor_to_quaternion(const VS& motor) {
        float scalar = motor[0];
        float b12 = motor[(1u<<0)|(1u<<1)];
        float b23 = motor[(1u<<1)|(1u<<2)];
        float b31 = motor[(1u<<2)|(1u<<0)];
        float norm = std::sqrt(b12*b12 + b23*b23 + b31*b31);
        if (norm < 1e-6f) return {0,0,0,1};
        float angle = 2.0f * std::acos(scalar);
        float sin_half = std::sin(angle * 0.5f);
        return { -b23 / sin_half, -b31 / sin_half, -b12 / sin_half, scalar };
    }
}

// ==================== CGA3D ====================
namespace CGA3D_impl {
    using Sig = CGA3D;
    using MV = Multivector<Sig>;
    using VS = Versor<Sig>;

    // Базисные индексы: e1=1, e2=2, e3=4, e0=8, e∞=16
    static constexpr size_t e1 = 1u << 0;
    static constexpr size_t e2 = 1u << 1;
    static constexpr size_t e3 = 1u << 2;
    static constexpr size_t e0 = 1u << 3;
    static constexpr size_t einf = 1u << 4;

    inline MV point(float x, float y, float z) {
        // p = e0 + x*e1 + y*e2 + z*e3 + 0.5*(x^2+y^2+z^2)*e∞
        MV p;
        p[e0] = 1.0f;
        p[e1] = x;
        p[e2] = y;
        p[e3] = z;
        p[einf] = 0.5f * (x*x + y*y + z*z);
        return p;
    }

        inline std::array<float, 3> extract_point(const MV& p) {
            float w = p[1u << 3];
            if (std::abs(w) < 1e-6f) return {0.0f, 0.0f, 0.0f};
            return { p[1u << 0] / w, p[1u << 1] / w, p[1u << 2] / w };
        }

    inline MV vector(float x, float y, float z) {
        // Направление как разность двух точек
        return point(x,y,z) - point(0,0,0);
    }

    inline MV sphere(float x, float y, float z, float radius) {
        // Сфера: точка - 0.5 * r^2 * e∞
        MV s = point(x, y, z);
        s[einf] -= 0.5f * radius * radius;
        return s;
    }

    inline VS translator(const std::array<float, 3>& t) {
        // T = 1 - 0.5 * (t_x e1 + t_y e2 + t_z e3) ∧ e∞
        VS T;
        T[0] = 1.0f;
        size_t blade = 0;
        blade = e1 | einf; T[blade] = -0.5f * t[0];
        blade = e2 | einf; T[blade] = -0.5f * t[1];
        blade = e3 | einf; T[blade] = -0.5f * t[2];
        return T;
    }

    inline VS rotor(const std::array<float, 3>& axis, float angle) {
        // R = cos(θ/2) - sin(θ/2) B, где B - бивектор плоскости вращения
        float len = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        if (len < 1e-6f) return VS{};
        std::array<float, 3> n = {axis[0]/len, axis[1]/len, axis[2]/len};
        MV B;
        B[(1u<<0)|(1u<<1)] = -n[2];
        B[(1u<<1)|(1u<<2)] = -n[0];
        B[(1u<<2)|(1u<<0)] = -n[1];
        B = B * (angle * 0.5f);
        return VS::from_bivector(B);
    }

    inline VS motor(const std::array<float, 3>& axis, float angle, const std::array<float, 3>& translation) {
        VS R = rotor(axis, angle);
        VS T = translator(translation);
        return T * R;
    }
}

// ==================== GA11 ====================
namespace GA11_impl {
    using Sig = GA11;
    using MV = Multivector<Sig>;
    using VS = Versor<Sig>;

    // Базис: e+ = 1 (положительная сигнатура), e- = 2 (отрицательная)
    static constexpr size_t e_plus = 1u << 0;
    static constexpr size_t e_minus = 1u << 1;

    inline MV vec(float a, float b) {
        MV v;
        v[e_plus] = a;
        v[e_minus] = b;
        return v;
    }

    inline std::pair<float, float> extract_vec(const MV& v) {
        return { v[e_plus], v[e_minus] };
    }

    // Лоренцево вращение (boost) в 2D пространстве-времени
    inline VS boost(float rapidity) {
        // B = rapidity * e+ ∧ e-
        MV B;
        B[e_plus | e_minus] = rapidity;
        return VS::from_bivector(B);
    }
}

} // namespace arxglue::ga