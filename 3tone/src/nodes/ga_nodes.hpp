// src/nodes/ga_nodes.hpp
#pragma once

#include "../core/node.hpp"
#include "../math/ga/multivector.hpp"
#include "../math/ga/versor.hpp"
#include "../math/ga/conversions.hpp"
#include "node_factory.hpp"
#include <nlohmann/json.hpp>

namespace arxglue {

// ========== Константный мультивектор ==========
template<typename Sig>
class ConstantMultivectorNode : public INode {
public:
    using MV = ga::Multivector<Sig>;
    ConstantMultivectorNode(const MV& value = MV{}) : m_value(value) {}

    void execute(Context& ctx) override {
        setOutputValue(ctx, 0, m_value);
    }

    ComponentMetadata getMetadata() const override {
        return {"ConstantMultivector", {}, {{"out", typeid(MV)}}, true, false};
    }

    void setParameter(const std::string& name, const std::any& value) override {
        if (name == "value") m_value = std::any_cast<MV>(value);
    }

    std::any getParameter(const std::string& name) const override {
        if (name == "value") return m_value;
        return {};
    }

    void serialize(nlohmann::json& j) const override {
        j["type"] = "ConstantMultivector";
        nlohmann::json coeffs = nlohmann::json::array();
        for (float c : m_value.coeffs()) coeffs.push_back(c);
        j["params"]["coeffs"] = coeffs;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("params") && j["params"].contains("coeffs")) {
            auto arr = j["params"]["coeffs"];
            typename MV::CoeffArray coeffs;
            for (size_t i = 0; i < MV::size; ++i) coeffs[i] = arr[i].get<float>();
            m_value = MV(coeffs);
        }
    }

protected:
    size_t computeParamsHash() const override {
        size_t h = 0;
        for (float c : m_value.coeffs()) {
            h ^= std::hash<float>{}(c) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

private:
    MV m_value;
};

// ========== Геометрическое произведение ==========
template<typename Sig>
class GeometricProductNode : public INode {
public:
    using MV = ga::Multivector<Sig>;

    void execute(Context& ctx) override {
        MV a = std::any_cast<MV>(getInputValue(ctx, 0));
        MV b = std::any_cast<MV>(getInputValue(ctx, 1));
        MV result = a * b;
        setOutputValue(ctx, 0, result);
    }

    ComponentMetadata getMetadata() const override {
        return {"GeometricProduct",
                {{"a", typeid(MV)}, {"b", typeid(MV)}},
                {{"result", typeid(MV)}},
                true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "GeometricProduct"; }
    void deserialize(const nlohmann::json&) override {}
};

// ========== Внешнее произведение ==========
template<typename Sig>
class WedgeNode : public INode {
public:
    using MV = ga::Multivector<Sig>;

    void execute(Context& ctx) override {
        MV a = std::any_cast<MV>(getInputValue(ctx, 0));
        MV b = std::any_cast<MV>(getInputValue(ctx, 1));
        MV result = a ^ b;
        setOutputValue(ctx, 0, result);
    }

    ComponentMetadata getMetadata() const override {
        return {"Wedge",
                {{"a", typeid(MV)}, {"b", typeid(MV)}},
                {{"result", typeid(MV)}},
                true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "Wedge"; }
    void deserialize(const nlohmann::json&) override {}
};

// ========== Применение версора ==========
template<typename Sig>
class VersorTransformNode : public INode {
public:
    using MV = ga::Multivector<Sig>;
    using VS = ga::Versor<Sig>;

    void execute(Context& ctx) override {
        VS motor = std::any_cast<VS>(getInputValue(ctx, 0));
        MV obj = std::any_cast<MV>(getInputValue(ctx, 1));
        MV transformed = motor.apply(obj);
        setOutputValue(ctx, 0, transformed);
    }

    ComponentMetadata getMetadata() const override {
        return {"VersorTransform",
                {{"motor", typeid(VS)}, {"object", typeid(MV)}},
                {{"transformed", typeid(MV)}},
                true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "VersorTransform"; }
    void deserialize(const nlohmann::json&) override {}
};

// ========== Конструирование мотора PGA3D ==========
class ConstructMotorPGA : public INode {
public:
    using VS = ga::Versor<ga::PGA3D>;

    void execute(Context& ctx) override {
        auto axis = std::any_cast<std::array<float,3>>(getInputValue(ctx, 0));
        float angle = std::any_cast<float>(getInputValue(ctx, 1));
        auto trans = std::any_cast<std::array<float,3>>(getInputValue(ctx, 2));
        auto motor = ga::PGA3D_impl::motor_from_axis_angle(axis, angle, trans);
        setOutputValue(ctx, 0, motor);
    }

    ComponentMetadata getMetadata() const override {
        return {"ConstructMotorPGA",
                {{"axis", typeid(std::array<float,3>)},
                 {"angle", typeid(float)},
                 {"translation", typeid(std::array<float,3>)}},
                {{"motor", typeid(VS)}},
                true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "ConstructMotorPGA"; }
    void deserialize(const nlohmann::json&) override {}
};

// ========== SLERP для версоров ==========
template<typename Sig>
class SlerpNode : public INode {
public:
    using VS = ga::Versor<Sig>;

    void execute(Context& ctx) override {
        VS v1 = std::any_cast<VS>(getInputValue(ctx, 0));
        VS v2 = std::any_cast<VS>(getInputValue(ctx, 1));
        float t = std::any_cast<float>(getInputValue(ctx, 2));
        VS result = VS::slerp(v1, v2, t);
        setOutputValue(ctx, 0, result);
    }

    ComponentMetadata getMetadata() const override {
        return {"Slerp",
                {{"v1", typeid(VS)}, {"v2", typeid(VS)}, {"t", typeid(float)}},
                {{"result", typeid(VS)}},
                true, false};
    }

    void setParameter(const std::string&, const std::any&) override {}
    std::any getParameter(const std::string&) const override { return {}; }
    void serialize(nlohmann::json& j) const override { j["type"] = "Slerp"; }
    void deserialize(const nlohmann::json&) override {}
};

// ========== Регистрация всех GA узлов ==========
inline void registerGANodes() {
    auto& factory = NodeFactory::instance();
    
    // PGA3D
    factory.registerNode("ConstantMultivectorPGA", []() {
        return std::make_unique<ConstantMultivectorNode<ga::PGA3D>>();
    });
    factory.registerNode("GeometricProductPGA", []() {
        return std::make_unique<GeometricProductNode<ga::PGA3D>>();
    });
    factory.registerNode("WedgePGA", []() {
        return std::make_unique<WedgeNode<ga::PGA3D>>();
    });
    factory.registerNode("VersorTransformPGA", []() {
        return std::make_unique<VersorTransformNode<ga::PGA3D>>();
    });
    factory.registerNode("ConstructMotorPGA", []() {
        return std::make_unique<ConstructMotorPGA>();
    });
    factory.registerNode("SlerpPGA", []() {
        return std::make_unique<SlerpNode<ga::PGA3D>>();
    });

    // CGA3D
    factory.registerNode("ConstantMultivectorCGA", []() {
        return std::make_unique<ConstantMultivectorNode<ga::CGA3D>>();
    });
    factory.registerNode("GeometricProductCGA", []() {
        return std::make_unique<GeometricProductNode<ga::CGA3D>>();
    });
    factory.registerNode("WedgeCGA", []() {
        return std::make_unique<WedgeNode<ga::CGA3D>>();
    });
    factory.registerNode("VersorTransformCGA", []() {
        return std::make_unique<VersorTransformNode<ga::CGA3D>>();
    });
    factory.registerNode("SlerpCGA", []() {
        return std::make_unique<SlerpNode<ga::CGA3D>>();
    });

    // GA11
    factory.registerNode("ConstantMultivectorGA11", []() {
        return std::make_unique<ConstantMultivectorNode<ga::GA11>>();
    });
    factory.registerNode("GeometricProductGA11", []() {
        return std::make_unique<GeometricProductNode<ga::GA11>>();
    });
    factory.registerNode("WedgeGA11", []() {
        return std::make_unique<WedgeNode<ga::GA11>>();
    });
    factory.registerNode("VersorTransformGA11", []() {
        return std::make_unique<VersorTransformNode<ga::GA11>>();
    });
    factory.registerNode("SlerpGA11", []() {
        return std::make_unique<SlerpNode<ga::GA11>>();
    });
}

} // namespace arxglue