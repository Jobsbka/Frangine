#pragma once
#include <any>
#include <typeindex>
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <memory>

namespace arxglue {

enum class TypeId : uint32_t {
    Unknown = 0,
    Int,
    Float,
    String,
    Texture,
    Mesh,
    Material,
    Scene
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
        // Используем emplace вместо operator[] (operator[] требует default-constructible)
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
        throw std::runtime_error("No conversion available from type " + std::to_string(static_cast<int>(from)) +
                                 " to " + std::to_string(static_cast<int>(to)));
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

private:
    struct TypeInfo {
        std::type_index stdType;
        TypeId parent;
        // Конструктор по умолчанию не нужен и не будет сгенерирован (std::type_index не default-constructible)
        TypeInfo(const std::type_index& t, TypeId p) : stdType(t), parent(p) {}
    };

    std::unordered_map<TypeId, TypeInfo> m_typeInfo;
    std::unordered_map<std::type_index, TypeId> m_typeToId;
    std::unordered_map<std::pair<TypeId, TypeId>, std::function<std::any(const std::any&)>, PairHash> m_converters;
};

inline void initBasicTypes() {
    auto& ts = TypeSystem::instance();
    ts.registerType(TypeId::Int, typeid(int));
    ts.registerType(TypeId::Float, typeid(float));
    ts.registerType(TypeId::String, typeid(std::string));
    ts.registerType(TypeId::Texture, typeid(std::shared_ptr<class TextureAsset>));
    ts.registerType(TypeId::Mesh, typeid(std::shared_ptr<class MeshAsset>));

    ts.registerConverter(TypeId::Int, TypeId::Float, [](const std::any& v) -> std::any {
        return static_cast<float>(std::any_cast<int>(v));
    });
    ts.registerConverter(TypeId::Float, TypeId::Int, [](const std::any& v) -> std::any {
        return static_cast<int>(std::any_cast<float>(v));
    });
}

} // namespace arxglue