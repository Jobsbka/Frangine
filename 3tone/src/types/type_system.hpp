// src/types/type_system.hpp
#pragma once
#include <any>
#include <typeindex>
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <array>
#include <cassert>
#include <cmath>

// GA headers (provide full definitions)
#include "../math/ga/signature.hpp"
#include "../math/ga/multivector.hpp"
#include "../math/ga/versor.hpp"
#include "../math/ga/conversions.hpp"

// Forward declarations for render types
namespace arxglue::render {
    class Texture;
    class Mesh;
    class Material;
    class Scene;
    class Camera;
}

namespace arxglue {

class TextureAsset;
class MeshAsset;

enum class TypeId : uint32_t {
    Unknown = 0,
    Int,
    Float,
    String,
    Float3,
    Matrix4,
    Quaternion,
    Texture,
    Mesh,
    Material,
    Scene,
    Camera,
    Multivector_PGA3D,
    Multivector_CGA3D,
    Multivector_GA11,
    Versor_PGA3D,
    Versor_CGA3D
};

struct PairHash {
    template<typename T1, typename T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

class TypeSystem {
public:
    static TypeSystem& instance() {
        static TypeSystem inst;
        return inst;
    }

    void registerType(TypeId id, const std::type_index& stdType, TypeId parent = TypeId::Unknown) {
        m_typeInfo.emplace(id, TypeInfo(stdType, parent));
        m_typeToId[stdType] = id;
    }

    TypeId getTypeId(const std::type_index& type) const {
        auto it = m_typeToId.find(type);
        if (it != m_typeToId.end()) return it->second;
        return TypeId::Unknown;
    }

    bool canConvert(TypeId from, TypeId to) const {
        if (from == to) return true;
        auto it = m_typeInfo.find(from);
        if (it == m_typeInfo.end()) return false;
        TypeId cur = it->second.parent;
        while (cur != TypeId::Unknown) {
            if (cur == to) return true;
            auto pit = m_typeInfo.find(cur);
            if (pit == m_typeInfo.end()) break;
            cur = pit->second.parent;
        }
        return m_converters.find({from, to}) != m_converters.end();
    }

    std::any convert(const std::any& value, TypeId from, TypeId to) const {
        if (from == to) return value;
        auto key = std::make_pair(from, to);
        auto it = m_converters.find(key);
        if (it != m_converters.end()) {
            return it->second(value);
        }
        auto infoIt = m_typeInfo.find(from);
        if (infoIt != m_typeInfo.end() && infoIt->second.parent != TypeId::Unknown) {
            return convert(value, infoIt->second.parent, to);
        }
        throw std::runtime_error("No conversion available");
    }

    std::any convertTo(const std::any& value, const std::type_index& targetType) const {
        TypeId fromId = getTypeId(value.type());
        TypeId toId = getTypeId(targetType);
        if (fromId == TypeId::Unknown || toId == TypeId::Unknown) {
            throw std::runtime_error("Unknown type(s) in conversion");
        }
        return convert(value, fromId, toId);
    }

    void registerConverter(TypeId from, TypeId to, std::function<std::any(const std::any&)> conv) {
        m_converters[{from, to}] = std::move(conv);
    }

    std::type_index getStdTypeIndex(TypeId id) const {
        auto it = m_typeInfo.find(id);
        if (it != m_typeInfo.end()) return it->second.stdType;
        return typeid(void);
    }

    static bool compareAny(const std::any& a, const std::any& b) {
        if (!a.has_value() && !b.has_value()) return true;
        if (!a.has_value() || !b.has_value()) return false;
        if (std::type_index(a.type()) != std::type_index(b.type())) return false;
        const std::type_info& ti = a.type();
        if (ti == typeid(int)) {
            return std::any_cast<int>(a) == std::any_cast<int>(b);
        } else if (ti == typeid(float)) {
            return std::any_cast<float>(a) == std::any_cast<float>(b);
        } else if (ti == typeid(std::string)) {
            return std::any_cast<std::string>(a) == std::any_cast<std::string>(b);
        } else if (ti == typeid(bool)) {
            return std::any_cast<bool>(a) == std::any_cast<bool>(b);
        }
        return false;
    }

private:
    struct TypeInfo {
        std::type_index stdType;
        TypeId parent;
        TypeInfo(const std::type_index& t, TypeId p) : stdType(t), parent(p) {}
    };

    std::unordered_map<TypeId, TypeInfo> m_typeInfo;
    std::unordered_map<std::type_index, TypeId> m_typeToId;
    std::unordered_map<std::pair<TypeId, TypeId>, std::function<std::any(const std::any&)>, PairHash> m_converters;
};

inline void initBasicTypes() {
    auto& ts = TypeSystem::instance();

    // Basic types
    ts.registerType(TypeId::Int, typeid(int));
    ts.registerType(TypeId::Float, typeid(float));
    ts.registerType(TypeId::String, typeid(std::string));
    ts.registerType(TypeId::Float3, typeid(std::array<float,3>));
    ts.registerType(TypeId::Matrix4, typeid(std::array<float,16>));
    ts.registerType(TypeId::Quaternion, typeid(std::array<float,4>));

    // Asset types
    ts.registerType(TypeId::Texture, typeid(std::shared_ptr<TextureAsset>));
    ts.registerType(TypeId::Mesh, typeid(std::shared_ptr<MeshAsset>));

    // Render types
    ts.registerType(TypeId::Texture, typeid(std::shared_ptr<render::Texture>));
    ts.registerType(TypeId::Mesh, typeid(std::shared_ptr<render::Mesh>));
    ts.registerType(TypeId::Material, typeid(std::shared_ptr<render::Material>));
    ts.registerType(TypeId::Scene, typeid(std::shared_ptr<render::Scene>));
    ts.registerType(TypeId::Camera, typeid(std::shared_ptr<render::Camera>));

    // GA types
    ts.registerType(TypeId::Multivector_PGA3D, typeid(ga::Multivector<ga::PGA3D>));
    ts.registerType(TypeId::Multivector_CGA3D, typeid(ga::Multivector<ga::CGA3D>));
    ts.registerType(TypeId::Multivector_GA11, typeid(ga::Multivector<ga::GA11>));
    ts.registerType(TypeId::Versor_PGA3D, typeid(ga::Versor<ga::PGA3D>));
    ts.registerType(TypeId::Versor_CGA3D, typeid(ga::Versor<ga::CGA3D>));

    // Int <-> Float
    ts.registerConverter(TypeId::Int, TypeId::Float, [](const std::any& v) -> std::any {
        return static_cast<float>(std::any_cast<int>(v));
    });
    ts.registerConverter(TypeId::Float, TypeId::Int, [](const std::any& v) -> std::any {
        return static_cast<int>(std::any_cast<float>(v));
    });

    // ========== PGA3D conversions ==========
    ts.registerConverter(TypeId::Float3, TypeId::Multivector_PGA3D,
        [](const std::any& v) -> std::any {
            auto arr = std::any_cast<std::array<float,3>>(v);
            return ga::PGA3D_impl::point(arr[0], arr[1], arr[2]);
        });
    ts.registerConverter(TypeId::Multivector_PGA3D, TypeId::Float3,
        [](const std::any& v) -> std::any {
            auto mv = std::any_cast<ga::Multivector<ga::PGA3D>>(v);
            return ga::PGA3D_impl::extract_point(mv);
        });

    ts.registerConverter(TypeId::Float3, TypeId::Multivector_PGA3D,
        [](const std::any& v) -> std::any {
            auto arr = std::any_cast<std::array<float,3>>(v);
            return ga::PGA3D_impl::vector(arr[0], arr[1], arr[2]);
        });
    ts.registerConverter(TypeId::Multivector_PGA3D, TypeId::Float3,
        [](const std::any& v) -> std::any {
            auto mv = std::any_cast<ga::Multivector<ga::PGA3D>>(v);
            return ga::PGA3D_impl::extract_vector(mv);
        });

    ts.registerConverter(TypeId::Matrix4, TypeId::Versor_PGA3D,
        [](const std::any& v) -> std::any {
            auto mat = std::any_cast<std::array<float,16>>(v);
            return ga::PGA3D_impl::motor_from_matrix(mat);
        });
    ts.registerConverter(TypeId::Versor_PGA3D, TypeId::Matrix4,
        [](const std::any& v) -> std::any {
            auto motor = std::any_cast<ga::Versor<ga::PGA3D>>(v);
            return ga::PGA3D_impl::motor_to_matrix(motor);
        });

    ts.registerConverter(TypeId::Quaternion, TypeId::Versor_PGA3D,
        [](const std::any& v) -> std::any {
            auto q = std::any_cast<std::array<float,4>>(v);
            return ga::PGA3D_impl::motor_from_quaternion(q[0], q[1], q[2], q[3]);
        });
    ts.registerConverter(TypeId::Versor_PGA3D, TypeId::Quaternion,
        [](const std::any& v) -> std::any {
            auto motor = std::any_cast<ga::Versor<ga::PGA3D>>(v);
            return ga::PGA3D_impl::motor_to_quaternion(motor);
        });

    // ========== CGA3D conversions ==========
    ts.registerConverter(TypeId::Float3, TypeId::Multivector_CGA3D,
        [](const std::any& v) -> std::any {
            auto arr = std::any_cast<std::array<float,3>>(v);
            return ga::CGA3D_impl::point(arr[0], arr[1], arr[2]);
        });
    ts.registerConverter(TypeId::Multivector_CGA3D, TypeId::Float3,
        [](const std::any& v) -> std::any {
            auto mv = std::any_cast<ga::Multivector<ga::CGA3D>>(v);
            return ga::CGA3D_impl::extract_point(mv);
        });

    ts.registerConverter(TypeId::Matrix4, TypeId::Versor_CGA3D,
        [](const std::any& v) -> std::any {
            auto mat = std::any_cast<std::array<float,16>>(v);
            std::array<float,3> trans = {mat[12], mat[13], mat[14]};
            float trace = mat[0] + mat[5] + mat[10];
            float angle = std::acos(std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f));
            std::array<float,3> axis = {1,0,0};
            if (angle > 1e-6f) {
                float s = 1.0f / (2.0f * std::sin(angle));
                axis[0] = (mat[6] - mat[9]) * s;
                axis[1] = (mat[8] - mat[2]) * s;
                axis[2] = (mat[1] - mat[4]) * s;
            }
            return ga::CGA3D_impl::motor(axis, angle, trans);
        });
    ts.registerConverter(TypeId::Versor_CGA3D, TypeId::Matrix4,
        [](const std::any& v) -> std::any {
            auto motor = std::any_cast<ga::Versor<ga::CGA3D>>(v);
            using namespace ga::CGA3D_impl;
            MV origin = point(0,0,0);
            MV ex = vector(1,0,0);
            MV ey = vector(0,1,0);
            MV ez = vector(0,0,1);
            MV new_o = motor.apply(origin);
            MV new_ex = motor.apply(ex);
            MV new_ey = motor.apply(ey);
            MV new_ez = motor.apply(ez);
            auto t = extract_point(new_o);
            auto c0 = extract_point(new_ex);
            auto c1 = extract_point(new_ey);
            auto c2 = extract_point(new_ez);
            std::array<float,16> mat{};
            mat[0] = c0[0] - t[0]; mat[4] = c1[0] - t[0]; mat[8] = c2[0] - t[0]; mat[12] = t[0];
            mat[1] = c0[1] - t[1]; mat[5] = c1[1] - t[1]; mat[9] = c2[1] - t[1]; mat[13] = t[1];
            mat[2] = c0[2] - t[2]; mat[6] = c1[2] - t[2]; mat[10] = c2[2] - t[2]; mat[14] = t[2];
            mat[3] = 0; mat[7] = 0; mat[11] = 0; mat[15] = 1;
            return mat;
        });

    // ========== GA11 conversions ==========
    ts.registerConverter(TypeId::Float3, TypeId::Multivector_GA11,
        [](const std::any& v) -> std::any {
            auto arr = std::any_cast<std::array<float,3>>(v);
            return ga::GA11_impl::vec(arr[0], arr[1]);
        });
    ts.registerConverter(TypeId::Multivector_GA11, TypeId::Float3,
        [](const std::any& v) -> std::any {
            auto mv = std::any_cast<ga::Multivector<ga::GA11>>(v);
            auto p = ga::GA11_impl::extract_vec(mv);
            return std::array<float,3>{p.first, p.second, 0.0f};
        });
}

} // namespace arxglue