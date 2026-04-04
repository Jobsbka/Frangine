#ifndef FRABGINE_CORE_MATH_QUATERNION_HPP
#define FRABGINE_CORE_MATH_QUATERNION_HPP

#include "Vector.hpp"
#include <cmath>

namespace frabgine {

class Quaternion {
private:
    float w_, x_, y_, z_;

public:
    Quaternion() : w_(1.0f), x_(0.0f), y_(0.0f), z_(0.0f) {}
    
    Quaternion(float w, float x, float y, float z) 
        : w_(w), x_(x), y_(y), z_(z) {}
    
    Quaternion(const Vector3fNamed& axis, float angle) {
        float halfAngle = angle * 0.5f;
        float sinHalf = std::sin(halfAngle);
        w_ = std::cos(halfAngle);
        x_ = axis.x() * sinHalf;
        y_ = axis.y() * sinHalf;
        z_ = axis.z() * sinHalf;
    }
    
    float w() const { return w_; }
    float x() const { return x_; }
    float y() const { return y_; }
    float z() const { return z_; }
    
    float length() const {
        return std::sqrt(w_*w_ + x_*x_ + y_*y_ + z_*z_);
    }
    
    Quaternion normalized() const {
        float len = length();
        if (len > 0.0f) {
            return Quaternion(w_/len, x_/len, y_/len, z_/len);
        }
        return *this;
    }
    
    Quaternion conjugate() const {
        return Quaternion(w_, -x_, -y_, -z_);
    }
    
    Quaternion operator*(const Quaternion& other) const {
        return Quaternion(
            w_*other.w_ - x_*other.x_ - y_*other.y_ - z_*other.z_,
            w_*other.x_ + x_*other.w_ + y_*other.z_ - z_*other.y_,
            w_*other.y_ - x_*other.z_ + y_*other.w_ + z_*other.x_,
            w_*other.z_ + x_*other.y_ - y_*other.x_ + z_*other.w_
        );
    }
    
    Vector3fNamed rotateVector(const Vector3fNamed& v) const {
        Quaternion q(0, v.x(), v.y(), v.z());
        Quaternion result = (*this) * q * conjugate();
        return Vector3fNamed(result.x(), result.y(), result.z());
    }
    
    static Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
        float cosTheta = a.w_*b.w_ + a.x_*b.x_ + a.y_*b.y_ + a.z_*b.z_;
        
        Quaternion bCopy = b;
        if (cosTheta < 0.0f) {
            bCopy = Quaternion(-b.w_, -b.x_, -b.y_, -b.z_);
            cosTheta = -cosTheta;
        }
        
        float theta = std::acos(cosTheta);
        float sinTheta = std::sin(theta);
        
        if (std::abs(sinTheta) < 0.001f) {
            return Quaternion(
                a.w_ + t * (bCopy.w_ - a.w_),
                a.x_ + t * (bCopy.x_ - a.x_),
                a.y_ + t * (bCopy.y_ - a.y_),
                a.z_ + t * (bCopy.z_ - a.z_)
            ).normalized();
        }
        
        float ratioA = std::sin((1.0f - t) * theta) / sinTheta;
        float ratioB = std::sin(t * theta) / sinTheta;
        
        return Quaternion(
            ratioA * a.w_ + ratioB * bCopy.w_,
            ratioA * a.x_ + ratioB * bCopy.x_,
            ratioA * a.y_ + ratioB * bCopy.y_,
            ratioA * a.z_ + ratioB * bCopy.z_
        );
    }
};

} // namespace frabgine

#endif // FRABGINE_CORE_MATH_QUATERNION_HPP
