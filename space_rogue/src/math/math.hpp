#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

using sF32 = float;
using sInt = int32_t;
using sU32 = uint32_t;

struct sVector2 {
    sF32 x, y;
    constexpr sVector2(sF32 x = 0, sF32 y = 0) : x(x), y(y) {}
    sVector2 operator+(sVector2 const& o) const { return {x + o.x, y + o.y}; }
    sVector2 operator-(sVector2 const& o) const { return {x - o.x, y - o.y}; }
    sVector2 operator*(sF32 s) const { return {x * s, y * s}; }
    sF32 operator^(sVector2 const& o) const { return x * o.x + y * o.y; }
};

struct sVector30 {
    sF32 x, y, z;
    constexpr sVector30(sF32 x = 0, sF32 y = 0, sF32 z = 0) : x(x), y(y), z(z) {}
    sVector30 operator+(sVector30 const& o) const { return {x + o.x, y + o.y, z + o.z}; }
    sVector30 operator-(sVector30 const& o) const { return {x - o.x, y - o.y, z - o.z}; }
    sVector30 operator*(sF32 s) const { return {x * s, y * s, z * s}; }
    sF32 operator^(sVector30 const& o) const { return x*o.x + y*o.y + z*o.z; }
    sVector30 operator%(sVector30 const& o) const {
        return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x };
    }
    sF32 Length() const { return std::sqrt(x*x + y*y + z*z); }
    void Normalize() { sF32 l = Length(); if(l > 1e-6f) { x/=l; y/=l; z/=l; } }
};

struct sVector31 : public sVector30 {
    using sVector30::sVector30;
};

struct sVector4 {
    sF32 x, y, z, w;
    constexpr sVector4(sF32 x = 0, sF32 y = 0, sF32 z = 0, sF32 w = 1) : x(x), y(y), z(z), w(w) {}
};

struct sMatrix34 {
    sVector30 i, j, k;  // базис
    sVector31 l;        // перенос
    sMatrix34() : i(1,0,0), j(0,1,0), k(0,0,1), l(0,0,0) {}
    sMatrix34(sVector30 i_, sVector30 j_, sVector30 k_, sVector31 l_) : i(i_), j(j_), k(k_), l(l_) {}
    
    sMatrix34 operator*(sMatrix34 const& o) const {
        // Произведение матриц (сначала o, потом текущая): M = this * o
        sMatrix34 r;
        r.i.x = i.x*o.i.x + i.y*o.j.x + i.z*o.k.x;
        r.i.y = i.x*o.i.y + i.y*o.j.y + i.z*o.k.y;
        r.i.z = i.x*o.i.z + i.y*o.j.z + i.z*o.k.z;
        r.j.x = j.x*o.i.x + j.y*o.j.x + j.z*o.k.x;
        r.j.y = j.x*o.i.y + j.y*o.j.y + j.z*o.k.y;
        r.j.z = j.x*o.i.z + j.y*o.j.z + j.z*o.k.z;
        r.k.x = k.x*o.i.x + k.y*o.j.x + k.z*o.k.x;
        r.k.y = k.x*o.i.y + k.y*o.j.y + k.z*o.k.y;
        r.k.z = k.x*o.i.z + k.y*o.j.z + k.z*o.k.z;
        r.l.x = i.x*o.l.x + i.y*o.l.y + i.z*o.l.z + l.x;
        r.l.y = j.x*o.l.x + j.y*o.l.y + j.z*o.l.z + l.y;
        r.l.z = k.x*o.l.x + k.y*o.l.y + k.z*o.l.z + l.z;
        return r;
    }
    
    static sMatrix34 Identity() { return sMatrix34(); }
    static sMatrix34 Translate(sVector31 t) {
        return sMatrix34({1,0,0},{0,1,0},{0,0,1}, t);
    }
};

struct sMatrix44 {
    sVector4 i, j, k, l;
    sMatrix44() : i(1,0,0,0), j(0,1,0,0), k(0,0,1,0), l(0,0,0,1) {}
    sMatrix44(sVector4 i_, sVector4 j_, sVector4 k_, sVector4 l_) : i(i_), j(j_), k(k_), l(l_) {}
    
    static sMatrix44 Perspective(sF32 fovY, sF32 aspect, sF32 zNear, sF32 zFar) {
        sF32 tanHalfFov = std::tan(fovY * 0.5f);
        sMatrix44 m;
        m.i.x = 1.0f / (aspect * tanHalfFov);
        m.j.y = 1.0f / tanHalfFov;
        m.k.z = zFar / (zFar - zNear);
        m.k.w = 1.0f;
        m.l.z = -zNear * zFar / (zFar - zNear);
        m.l.w = 0.0f;
        return m;
    }
};